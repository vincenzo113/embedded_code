#include "fsm_logic.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#define SIZE 7 //password size must be of 6 + 1 for the terminator character
#define DOOR_CLOSED_CCR 8 //5% of arr
#define DOOR_OPEN_CCR 12 //7.5% of arr
#define INSERT_CODE_MSG "INSERISCI CODICE...\r\n"
#define transmit(msg) HAL_UART_Transmit_IT(&hlpuart1 ,(uint8_t *) msg , strlen(msg))
#define CODE 123456

/**
 * Enumeration machine's states
 */
typedef enum fsm_state_enum{
	IDLE = 0 ,
	INSERT_CODE = 1,
	ACCESS_GRANTED = 2 ,
	ACCESS_DENIED = 3 ,
	DOOR_OPEN =4
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
	//button state
	button_state_t button;
	button_state_t last_button;
	//timer
	uint8_t timer_elapsed;
	uint8_t timer_elapsed_first;
	//password inserted
	char passInserted[SIZE];
}FSM_input_t;


/**
 * FSM Main Structure
 * It is composed by the input and outputs as well as
 * the current status of the machine and the current input reads
 */
typedef struct FSM_s{
	button_t *button;
	timer_t  *timer;
	//digital outputs:
	led_t *lg,*lr;
	//state where is the machine
	fsm_state_t state;

	//inputs of the machine
	FSM_input_t in;

	//timer pwm utils
	timer_t *timer_pwm;

	//uart things
	char byte;
	int byteReady;
	int codeReady;
	int idx;

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


/*
 * Public init function
 */
int8_t FSM_init(button_t *button , timer_t *timer , led_t *lg , led_t *lr , timer_t *timer_pwm)
{
	if(!button || !timer || !lg || !lr || !timer_pwm) return FSM_ERR;

	fsm.button = button;
	fsm.timer = timer;
	fsm.lg = lg;
	fsm.lr = lr;
	fsm.timer_pwm = timer_pwm;
	//set behavior of peripherals , i.e. how long the press on button should be to consider it as valid ecc...
	if(button_set_delay(fsm.button , 200) != BUTTON_OK) return FSM_ERR;

	fsm.state = IDLE;
	//take the servo to 0 degrees , i.e. close the door
	if(timer_set_duty_x10(fsm.timer_pwm , TIM_CHANNEL_1 , 50) != TIMER_OK) return FSM_ERR;
	if(timer_start_pwm(fsm.timer_pwm , TIM_CHANNEL_1) != TIMER_OK) return FSM_ERR;
	//all led are already turned off since they are defaulted to this state when starting the program

	//defaul uart things
	fsm.idx = 0;
	fsm.codeReady = 0;
	fsm.byteReady = 0;
	fsm.in.passInserted[0] = '\0';

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

	//read button state
	fsm.in.last_button = fsm.in.button;
	//read new state for button
	if(button_read(fsm.button , &fsm.in.button) != BUTTON_OK) return FSM_ERR;

	//read timer elapsed
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);

	if(fsm.state != INSERT_CODE) return FSM_OK; //accumulate only when we are in the correct state to do that


	if(fsm.byteReady && !fsm.codeReady){
		fsm.byteReady = 0 ;

		//analyze byte received

		//user sent enter and there was at least one char inserted
		if(((fsm.byte == '\r') || (fsm.byte == '\n')) && fsm.idx !=0){
			fsm.in.passInserted[fsm.idx] = '\0';
			fsm.idx=0;
			fsm.codeReady=1;
		}
		//user sent a valid number
		else if(fsm.byte >= '0' && fsm.byte <= '9'){
			//echo through *
			transmit("*");
			fsm.in.passInserted[fsm.idx] = fsm.byte;
			fsm.idx++;
			if(fsm.idx == SIZE-1){
				fsm.in.passInserted[fsm.idx] = '\0';
				fsm.codeReady = 1;
				fsm.idx = 0;
			}
		}
	}

	return FSM_OK;
}



static int8_t FSM_stateIdle(){
	if(fsm.in.last_button != fsm.in.button && fsm.in.button == GPIO_PIN_SET){
		//switch to INSERT CODE STATE
		fsm.state = INSERT_CODE;
		//trigger receive of code
		fsm.byteReady = 0; //always to clean the first received in the last attempt
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *) &fsm.byte , 1);
		transmit(INSERT_CODE_MSG);
	}
	return FSM_OK;
}



static int8_t FSM_stateInsertCode(){
	if(!fsm.codeReady) return FSM_OK; //if code is not ready yet , continue waiting for the signal from the input read func

	int code = atoi(fsm.in.passInserted);
	fsm.codeReady = 0; //code is not ready anymore
	if(code == CODE){
		fsm.state = ACCESS_GRANTED;
		//entry actions for this state:
		//turn on led green
		if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
		//open door i.e. take the servo to 7.5% of arr and hence to 90 degrees
		if(timer_set_duty_x10(fsm.timer_pwm , TIM_CHANNEL_1 , 75) != TIMER_OK) return FSM_ERR;
		//start timer to track elapsing of 5 seconds
		if(timer_reset(fsm.timer) != TIMER_OK)return FSM_ERR;
		if(timer_set_period_ms(fsm.timer , 5000) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
	}
	else{
		fsm.state = ACCESS_DENIED;
		//entry actions:
		if(led_on(fsm.lr) != LED_OK) return FSM_ERR;
		if(timer_reset(fsm.timer) != TIMER_OK)return FSM_ERR;
		if(timer_set_period_ms(fsm.timer , 2000) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
	}

	return FSM_OK;
}


static int8_t FSM_stateAccessGranted(){
	//if 5 seconds are elapsed
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		//turn off green led
		if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
		//close the door
		if(timer_set_duty_x10(fsm.timer_pwm , TIM_CHANNEL_1 , 50) != TIMER_OK) return FSM_ERR;
		//stop the timer
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;


	}

	return FSM_OK;
}


static int8_t FSM_stateAccessDenied(){
	if(fsm.in.timer_elapsed){
		fsm.state = IDLE;
		if(led_off(fsm.lr)!= LED_OK) return FSM_ERR;
		//stop the timer since the state we are entering does not need it
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;

	}
	return FSM_OK;
}




static int8_t FSM_update_state(){


	switch(fsm.state)
	{
	case IDLE: {
		if(FSM_stateIdle() != FSM_OK) return FSM_ERR;
		break;
	}
	case INSERT_CODE: {
		if(FSM_stateInsertCode() != FSM_OK) return FSM_ERR;
		break;
	}

	case ACCESS_GRANTED: {
		if(FSM_stateAccessGranted()!=FSM_OK) return FSM_ERR;
		break;
	}

	case ACCESS_DENIED: {
		if(FSM_stateAccessDenied() != FSM_OK) return FSM_ERR;
		break;
	}



		default:
			return FSM_ERR;
			break;
	}
	return FSM_OK;
}










//********************************************************************************
//******	CALLBACKS (if needed)
//**************************************************************************

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.byteReady=1;

		if(!fsm.codeReady) HAL_UART_Receive_IT(&hlpuart1  , (uint8_t *) &fsm.byte , 1);


	}
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if(htim == &htim6){
		timer_period_elapsed(fsm.timer , &htim6);
	}
}


