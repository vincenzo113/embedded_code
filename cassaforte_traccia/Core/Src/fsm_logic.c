#include "fsm_logic.h"
#include "string.h"
#include "tim.h"
#include "usart.h"
#include "stdlib.h"
#define SIZE 5
#define CORRECT_CODE 4815
#define CHANNEL TIM_CHANNEL_1
#define CLOSE_DUTY 50
#define OPEN_DUTY 75 //7.5% equivalent to 90 degrees * 10



#define transmit(msg) HAL_UART_Transmit_IT(&hlpuart1 , (uint8_t * )msg , strlen(msg))
/**
 * Enumeration machine's states
 */
typedef enum fsm_state_enum{
IDLE=0, INSERT_PIN=1 , UNLOCKED = 2 ,WRONG_PIN=3 , LOCKOUT = 4 , OPEN = 5
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
button_state_t button;
button_state_t button_last;
uint8_t timer_elapsed;
uint8_t timer_elapsed_first;

//code inserted for uart to protect
char codeInserted[SIZE];
}FSM_input_t;


/**
 * FSM Main Structure
 * It is composed by the input and outputs as well as
 * the current status of the machine and the current input reads
 */
typedef struct FSM_s
{
button_t *button;
timer_t *timer;
timer_t *pwm;
led_t *lg , *lr;
FSM_input_t in;
fsm_state_t state;
//wrong trials
uint8_t wrongTrials;

//uart
uint8_t idx;
char byte;
uint8_t codeReady;
uint8_t byteReady;
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


/*
 * Private function to update the current status and the output
 */
static int8_t FSM_stateUnlocked();
static int8_t FSM_stateIdle();
static int8_t FSM_stateInsertPin();
static int8_t FSM_stateWrongPin();
static int8_t FSM_stateLockout();


/*
 * Public init function
 */
int8_t FSM_init(button_t *button , timer_t *timer, timer_t *pwm , led_t *lg , led_t *lr ){

	//guard
	if(!button || !lg || !lr || !timer || !pwm) return FSM_ERR;

	fsm.button = button;
	fsm.timer = timer;
	fsm.pwm = pwm;
	fsm.lg = lg;
	fsm.lr = lr;


	//initialization of machine
	fsm.state = IDLE;
	if(button_set_delay(fsm.button , 200) != BUTTON_OK) return FSM_ERR;
	if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
	if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	if(timer_set_period_ms(fsm.pwm , 20) != TIMER_OK) return FSM_ERR;
	if(timer_set_duty_x10(fsm.pwm , CHANNEL , CLOSE_DUTY) != TIMER_OK) return FSM_ERR;
	if(timer_start_pwm(fsm.pwm , CHANNEL) != TIMER_OK) return FSM_ERR;
	if(led_set_toggle_period(fsm.lr,2000) != LED_OK) return FSM_ERR;
	fsm.wrongTrials = 0;
	fsm.codeReady = 0;
	fsm.byteReady = 0;
	fsm.idx = 0;
	fsm.in.codeInserted[0] = '\0';

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
	fsm.in.button_last = fsm.in.button;
	if(button_read(fsm.button , &fsm.in.button) != BUTTON_OK) return FSM_ERR;
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);

	//guard to prevent accumulating in forbidden states
	if(fsm.state != INSERT_PIN) return FSM_OK;

	//accumulatin is allowed in this section
	if(fsm.byteReady && !fsm.codeReady){
		fsm.byteReady = 0; //byte is no more ready
		//user pressed enter
		if((fsm.byte == '\r' || fsm.byte == '\n') && (fsm.idx==0)){
			fsm.in.codeInserted[fsm.idx]='\0';
			fsm.idx = 0 ;
			fsm.codeReady=1;
		}


		if(fsm.byte >= '0' && fsm.byte <= '9'){
			transmit("#");
			fsm.in.codeInserted[fsm.idx] = fsm.byte;
			fsm.idx ++;
			if(fsm.idx == SIZE-1){
				fsm.in.codeInserted[fsm.idx] = '\0';
				fsm.idx =0;
				fsm.codeReady = 1;
			}
		}

	}

	return FSM_OK;
}

static int8_t FSM_update_state(){



	switch(fsm.state)
	{
	case IDLE: return FSM_stateIdle();

	case INSERT_PIN: return FSM_stateInsertPin();

	case UNLOCKED: return FSM_stateUnlocked();

	case LOCKOUT: return FSM_stateLockout();

	case WRONG_PIN: return FSM_stateWrongPin();
		default: return FSM_ERR;



	}
	return FSM_OK; ;
}


static int8_t FSM_stateIdle(){
	if(fsm.in.button==PRESSED && fsm.in.button != fsm.in.button_last)
	{
		fsm.state = INSERT_PIN;
		//entry actions:
		transmit("INSERIRE PIN: \r\n");
		fsm.byteReady = 0;
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t * )&fsm.byte ,1);

	}

	return FSM_OK;
}



static int8_t FSM_stateInsertPin(){
	if(!fsm.codeReady) return FSM_OK; //continue waiting

	fsm.codeReady=0; //code is confirmed as it is completed since there is no needing of a button to confirm from specs
	int code = atoi(fsm.in.codeInserted);


	if(code == CORRECT_CODE){
		fsm.state = UNLOCKED;
		fsm.wrongTrials=0;
		if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period_ms(fsm.timer , 5000) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_duty_x10(fsm.pwm, CHANNEL , OPEN_DUTY) != TIMER_OK) return FSM_ERR;
	}

	else
	{
		fsm.wrongTrials ++;

		if(fsm.wrongTrials == 3){
		fsm.state = LOCKOUT;
		fsm.wrongTrials = 0;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period(fsm.timer , 30) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		transmit("LOCKOUT...\r\n");
		}


		else{
		fsm.state = WRONG_PIN;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period_ms(fsm.timer , 3000) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_on(fsm.lr) != LED_OK) return FSM_ERR;
		transmit("WRONG PIN...\r\n");
		}
	}


	return FSM_OK;
}


static int8_t FSM_stateUnlocked(){
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_duty_x10(fsm.pwm , CHANNEL , CLOSE_DUTY) != TIMER_OK) return FSM_ERR;
	}

	return FSM_OK;
}



static int8_t FSM_stateLockout(){
	if(!fsm.in.timer_elapsed){
		if(led_toggle(fsm.lr) != LED_OK) return FSM_ERR;
	}


	else{
		fsm.state = IDLE;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	}

	return FSM_OK ;
}


static int8_t FSM_stateWrongPin(){

	if(fsm.in.timer_elapsed)
	{
		fsm.state = IDLE;
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
		if(timer_reset(fsm.timer)!=TIMER_OK) return FSM_ERR;
	}

	return FSM_OK ;
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
		fsm.byteReady=1; //a byte is correctly received and hence ready to be processed
		if(!fsm.codeReady) HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		if(huart->ErrorCode == HAL_UART_ERROR_ORE){
			 HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
		}
	}
}




