#include "fsm_logic.h"
#include "string.h"
#include "usart.h"
#include "tim.h"
#include "stdlib.h"
#include "queue.h"

#define SIZE 7
#define print(msg) queue_enqueue(fsm.queueMessages , msg)
/**
 * Enumeration machine's states
 */
typedef enum fsm_state_enum{
	IDLE= 0,
	INSERT_CODE	= 1,
	ACCESS_GRANTED= 2,
	ACCESS_DENIED = 3 ,
	DOOR_OPEN = 4
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
	button_state_t button;	 		//Stable value of the button at current step
	button_state_t button_last;		//Stable value of the button at previous step
	uint8_t timer_elapsed;
	uint8_t timer_elapsed_first;		//Used to avoid the first timer interrupt

	char codeInserted[SIZE];

}FSM_input_t;


/**
 * FSM Main Structure
 * It is composed by the input and outputs as well as
 * the current status of the machine and the current input reads
 */
typedef struct FSM_s
{
	// FSM Inputs devices
	button_t *button;
	timer_t *timer;

	// FSM Inputs state
	FSM_input_t in;

	// FSM Outputs devices
	led_t *lg, *lr;

	// FSM current status
	fsm_state_t state;

	//Cycle duration in ms 
	uint32_t cycle;

	int idx;
	int codeReady;
	int byteReady;
	char byte;


	//utility
	queue_t *queueMessages;
	int tx_busy;
} FSM_t;


/*
 * Private machine state
 */
static FSM_t fsm;


/*
 * Private function to read and buffers the inputs
 */
static int8_t FSM_read_inputs();


/*
 * Private function to update the current status and the output
 */
static int8_t FSM_update_state();
static int8_t FSM_stateIdle();
static int8_t FSM_stateInsertCode();
static int8_t FSM_stateAccessGranted();
static int8_t FSM_stateAccessDenied();





/*
 * Public init function
 */
int8_t FSM_init(uint32_t cycle , led_t* lg, led_t* lr, button_t* button, timer_t* timer , queue_t * queueMessages){
	// To be completed
	if(cycle && timer && lg &&lr && button){
		fsm.cycle = cycle;
		fsm.timer = timer;
		fsm.lg = lg ;
		fsm.lr = lr;
		fsm.button = button;
		fsm.queueMessages = queueMessages;
	}


	if(button_set_delay(fsm.button , 200) != BUTTON_OK) return FSM_ERR;
	fsm.tx_busy = 0 ;
	fsm.state = IDLE;
	if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
	if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	fsm.byteReady = 0;
	fsm.codeReady = 0;
	fsm.in.codeInserted[0]='\0';


return FSM_OK;
}

/*
 * Public step function
 */
int8_t FSM_step(){
	if(FSM_read_inputs() != FSM_OK) return FSM_ERR;
	if(FSM_update_state() != FSM_OK) return FSM_ERR;

	return FSM_OK;
}

void uart_tx_task(){
	static char msg[50];

	if(fsm.tx_busy) return;

	//if there is something to transmit
	if (queue_extract(fsm.queueMessages , msg) == QUEUE_OK){
		HAL_UART_Transmit_IT(&hlpuart1 , msg , strlen(msg));
		fsm.tx_busy = 1;
	}


}

//********************************************************************************
//**************************************************************************
//******	STATIC FUNCTIONS

static int8_t FSM_read_inputs(){
	fsm.in.button_last = fsm.in.button;
	if(button_read(fsm.button , &fsm.in.button) != BUTTON_OK) return FSM_ERR;
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);

	//accumulation
	if(fsm.state != INSERT_CODE) return FSM_OK;


	if(fsm.byteReady && !fsm.codeReady){
		fsm.byteReady = 0;
		print("*\0");
		if((fsm.byte== '\r' || fsm.byte == '\n') && fsm.idx != 0){
			fsm.in.codeInserted[fsm.idx] = '\0';
			fsm.idx = 0;
			fsm.codeReady=1;
		}

		else if (fsm.byte >= '0' && fsm.byte <='9'){
			fsm.in.codeInserted[fsm.idx] = fsm.byte ;
			fsm.idx++;
			if(fsm.idx == SIZE-1){
				fsm.in.codeInserted[fsm.idx] = '\0';
				fsm.idx = 0;
				fsm.codeReady = 1;
			}
		}
	}


	return FSM_OK;

}


static int8_t FSM_update_state(){
	switch(fsm.state){
	case IDLE:{
		FSM_stateIdle();
		break;
	}


	case INSERT_CODE: {
		FSM_stateInsertCode();
		break;
	}

	case ACCESS_GRANTED: {
		FSM_stateAccessGranted();
		break;
	}

	case ACCESS_DENIED: {
		FSM_stateAccessDenied();
		break;
	}



	default: return FSM_ERR;
	}

	return FSM_OK;
}


static int8_t FSM_stateIdle(){
	if(fsm.in.button == GPIO_PIN_SET && fsm.in.button != fsm.in.button_last){
		//go to insert code state and trigger accumulation
		fsm.state = INSERT_CODE;
		//entry actions:
		print("INSERISCI CODICE...\r\n\0");
		fsm.byteReady = 0;
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *) &fsm.byte , 1);



	}

	return FSM_OK;
}

static int8_t FSM_stateInsertCode(){

	if(fsm.codeReady){
		fsm.codeReady = 0;
		//check code is correct
		int accessDenied = strcmp(fsm.in.codeInserted, "123456") != 0;
		if(!accessDenied){
			fsm.state = ACCESS_GRANTED ;
			//entry actions to make:
			if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(timer_set_period(fsm.timer , 5) != TIMER_OK) return FSM_ERR;
			if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
			print("ACCESS GRANTED!\r\n\0");
			if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
		}
		else{
			fsm.state = ACCESS_DENIED;
			if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(timer_set_period(fsm.timer , 2) != TIMER_OK) return FSM_ERR;
			if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
			print("ACCESS DENIED...\r\n\0");
			if(led_on(fsm.lr) != LED_OK) return FSM_ERR;

		}

	}

	return FSM_OK;
}




static int8_t FSM_stateAccessGranted(){
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_off(fsm.lg) != LED_OK) return FSM_ERR;

	}
	return FSM_OK;
}



static int8_t FSM_stateAccessDenied(){
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;

	}
	return FSM_OK;
}








void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.tx_busy = 0;
	}
}



void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.byteReady =1 ;

		if(!fsm.codeReady) HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *) &fsm.byte , 1);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* handler) {
	if(handler == &htim6){
		timer_period_elapsed(fsm.timer , handler);
	}
}



//to prevent buffer overrun
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		HAL_UART_AbortReceive(huart);

		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *) &fsm.byte , 1);
	}
}


