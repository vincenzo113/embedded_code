#include "fsm_logic.h"
#include "string.h"
#include "tim.h"
#include "usart.h"
#include "stdlib.h"
#define SIZE 5 //4 more the terminator char
#define CHANNEL TIM_CHANNEL_1
#define CLOSED_DUTY 50
#define OPENED_DUTY 75
#define QUEUE_ITEM_SIZE 50
#define CORRECT_CODE 0000
#define MAX_TRIALS 3
#include "queue.h"

/**
 * Enumeration machine's states
 */
typedef enum fsm_state_enum{
IDLE =0 , INSERT_CODE = 1, ACCESS_GRANTED = 2 , DOOR_OPEN = 3, ACCESS_DENIED = 4 , LOCKOUT = 5 , TAMPER_ALARM = 6
} fsm_state_t;

/*
 * The FSM is a Moore machine that is updated at each step cycle
 * The input are read before evaluating state and output changes
 * therefore, we need to store the value from each input device at a given cycle
 *
 * This structure represent the state read from the input devices at each cycle
 * it buffers the state of each input ensuring that is stable for the overall cycle duration
 */
typedef struct FSM_input_s{
button_state_t pUtente;
button_state_t pUtente_last;
uint8_t timer_elapsed;
uint8_t timer_elapsed_first;

//code
char codeInserted[SIZE];
}FSM_input_t;


/**
 * FSM Main Structure
 * It is composed by the input and outputs as well as
 * the current status of the machine and the current input reads
 */
typedef struct FSM_s
{
	button_t *pUtente;
	timer_t *timer;
	timer_t *pwm;
	led_t *lg , *lr;
	FSM_input_t in;
	fsm_state_t state;
	//uart
	char byte;
	uint8_t byteReady;
	uint8_t codeReady;
	uint8_t idx;
	//trials
	uint8_t wrongTrials;
	//queue to append messages
	queue_t *queue;
	uint8_t uart_busy;
	char tx_buffer[QUEUE_ITEM_SIZE];

} FSM_t;





/*
 * Private machine state
 */
static FSM_t fsm;


/*
 * Private function to read and buffers the inputs
 */
static int8_t FSM_read_inputs();
static int8_t FSM_update_state();



static int8_t transmit(const char *msg){
	if(queue_enqueue(fsm.queue , msg) != QUEUE_OK) return FSM_ERR;
	return FSM_OK;
}

/*
 * Private function to update the current status and the output
 */
static int8_t FSM_stateIdle();
static int8_t FSM_stateInsertCode();
static int8_t FSM_stateAccessGranted();
static int8_t FSM_stateAccessDenied();
static int8_t FSM_stateLockout();
static int8_t FSM_stateTamperAlarm();

/*
 * Public init function
 */
int8_t FSM_init(button_t *pUtente , timer_t *timer ,timer_t *pwm , led_t *lg , led_t *lr , queue_t *queue){

	if(!pUtente || !timer || !pwm || !lg || !lr || !queue) return FSM_ERR;

	fsm.pUtente = pUtente;
	fsm.timer = timer;
	fsm.pwm = pwm;
	fsm.lg = lg;
	fsm.lr = lr;
	fsm.queue = queue;



	fsm.state = IDLE;
	if(button_set_delay(fsm.pUtente , 200) != BUTTON_OK) return FSM_ERR;
	if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
	if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	if(led_set_toggle_period(fsm.lr , 3) != LED_OK) return FSM_ERR;
	if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
	if(timer_set_period_ms(fsm.pwm , 20) != TIMER_OK) return FSM_ERR;
	if(timer_set_duty_x10(fsm.pwm , CHANNEL ,CLOSED_DUTY) != TIMER_OK) return FSM_ERR;
	if(timer_start_pwm(fsm.pwm , CHANNEL) != TIMER_OK) return FSM_ERR;
	fsm.idx = 0 ;
	fsm.codeReady = 0;
	fsm.byteReady = 0;
	fsm.in.codeInserted[0] = '\0';
	fsm.uart_busy =0;
	transmit("IDLE STATE,machine initialized\r\n");
	return FSM_OK;
}

int8_t consume_task(){
	if(queue_is_empty(fsm.queue) == QUEUE_EMPTY || fsm.uart_busy ) return FSM_OK;
	if(queue_extract(fsm.queue , fsm.tx_buffer) != QUEUE_OK) return FSM_ERR;
	HAL_UART_Transmit_IT(&hlpuart1 , (uint8_t *)fsm.tx_buffer ,strlen(fsm.tx_buffer));
	fsm.uart_busy=1;
	return FSM_OK;
}

/*
 * Public step function
 */
int8_t FSM_step(){
	int8_t res = FSM_ERR;
	uint32_t cycle_start = 0;
	uint32_t cycle_runtime = 0;

	cycle_start = HAL_GetTick();

	if(FSM_read_inputs() == FSM_OK){
		res = FSM_OK;
	}

	if( (res == FSM_OK) && (FSM_update_state() != FSM_OK) ){
		res = FSM_ERR;
	}

	cycle_runtime = HAL_GetTick() - cycle_start;

	if(FSM_CYCLE_DURATION > cycle_runtime)
	{
		HAL_Delay(FSM_CYCLE_DURATION - cycle_runtime);
	}
	else
	{
		res = FSM_ERR;
	}

	return res;
}

//********************************************************************************
//******	STATIC FUNCTIONS
//**************************************************************************

static int8_t FSM_read_inputs(){
	fsm.in.pUtente_last = fsm.in.pUtente;
	if(button_read(fsm.pUtente , &fsm.in.pUtente) != BUTTON_OK) return FSM_ERR;
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);

	if(fsm.state != INSERT_CODE) return FSM_OK;

	if(fsm.byteReady && !fsm.codeReady){
		fsm.byteReady=0; //consume the byte when we read it here

		if(((fsm.byte == '\r') || (fsm.byte == '\n') )&& fsm.idx !=0 ){
			fsm.in.codeInserted[fsm.idx] = '\0';
			fsm.idx =0;
			fsm.codeReady =1;
		}

		if(fsm.byte >= '0' && fsm.byte <= '9'){
			fsm.in.codeInserted[fsm.idx] = fsm.byte;
			fsm.idx ++;
			if(fsm.idx == SIZE-1){
				fsm.in.codeInserted[fsm.idx] = '\0';
				fsm.idx =0 ;
				fsm.codeReady = 1;
			}
		}

	}



	return FSM_OK;
}




static int8_t FSM_stateIdle(){
	if(fsm.in.pUtente == PRESSED && fsm.in.pUtente != fsm.in.pUtente_last){
		fsm.state = INSERT_CODE;
		//entry actions:
		fsm.byteReady=0;
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
		if(timer_set_period(fsm.timer , 10) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		transmit("GOING INTO INSERT CODE STATE , waiting for insert\r\n");
	}
	return FSM_OK;
}



static int8_t FSM_stateInsertCode(){
	//timeout
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		transmit("TOO LONG TIME TO INSERT CODE , GOING BACK TO IDLE\r\n");
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		fsm.idx =0 ; //reset if code was not completed since in read inputs we reset it only when completing the code
		return FSM_OK;
	}

	//timer is not elapsed hence check if code is ready
	if(!fsm.codeReady) return FSM_OK; //continue waiting


	//code is ready and timer is not elapsed
	fsm.codeReady=0;
	int code = atoi(fsm.in.codeInserted);
	if(code == CORRECT_CODE){
		fsm.state = ACCESS_GRANTED;
		if(timer_reset(fsm.timer)  != TIMER_OK) return FSM_ERR;
		if(timer_set_period(fsm.timer , 5) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
		if(timer_set_duty_x10(fsm.pwm , CHANNEL , OPENED_DUTY)  != TIMER_OK) return FSM_ERR;
		fsm.wrongTrials=0;
		transmit("UNLOCKING DOOR...\r\n");
	}

	else{
		fsm.wrongTrials ++;

		if(fsm.wrongTrials == MAX_TRIALS){
			fsm.state = LOCKOUT;
			if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(timer_set_period(fsm.timer , 15) != TIMER_OK) return FSM_ERR;
			if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
			transmit("ENTERING LOCKOUT...\r\n");
			fsm.wrongTrials=0;
		}

		else
		{
			fsm.state = ACCESS_DENIED;

			if(led_on(fsm.lr)!= LED_OK) return FSM_ERR;
			if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(timer_set_period(fsm.timer , 5) != TIMER_OK) return FSM_ERR;
			if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
			transmit("ACCESS DENIED\r\n");
		}
	}


return FSM_OK;
}


static int8_t FSM_stateAccessGranted(){
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
		if(timer_set_duty_x10(fsm.pwm , CHANNEL , CLOSED_DUTY) != TIMER_OK) return FSM_ERR;
	}

	return FSM_OK;
}
static int8_t FSM_stateAccessDenied(){

	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	}

	return FSM_OK;
}
static int8_t FSM_stateLockout(){

	if(fsm.in.timer_elapsed){
	fsm.state = IDLE;
	if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
	return FSM_OK;
	}


	if(fsm.in.pUtente == PRESSED && fsm.in.pUtente != fsm.in.pUtente_last){
		fsm.state = TAMPER_ALARM;
		transmit("ALLARME MANOMISSIONE\r\n");
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period(fsm.timer , 15) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
		if(led_set_toggle_period(fsm.lr , 1) != LED_OK) return FSM_ERR;
		return FSM_OK;
	}


	if(led_toggle(fsm.lr) != LED_OK) return FSM_ERR;



	return FSM_OK;

}
static int8_t FSM_stateTamperAlarm(){
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		return FSM_OK;
	}

	//timer is not elapsed
	if(led_toggle(fsm.lr) != LED_OK) return FSM_ERR;


	return FSM_OK;
}



static int8_t FSM_update_state(){
	switch(fsm.state)
	{
	case IDLE: return FSM_stateIdle();
	case INSERT_CODE: return FSM_stateInsertCode();
	case ACCESS_GRANTED: return FSM_stateAccessGranted();
	case ACCESS_DENIED: return FSM_stateAccessDenied();
	case LOCKOUT: return FSM_stateLockout();
	case TAMPER_ALARM: return FSM_stateTamperAlarm();
	default: return FSM_ERR;


	}
	return FSM_OK;
}

//********************************************************************************
//******	CALLBACKS (if needed)
//**************************************************************************


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if(htim == &htim6){
		timer_period_elapsed(fsm.timer , htim);
	}
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.byteReady = 1;
		if(!fsm.codeReady) HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *) &fsm.byte ,1);
	}
}



void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.uart_busy=0;
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		if(huart->ErrorCode == HAL_UART_ERROR_ORE) HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte  , 1);
	}
}




