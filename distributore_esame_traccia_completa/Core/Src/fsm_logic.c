#include "fsm_logic.h"
#include "string.h"
#include "usart.h"
#include "tim.h"
#include "stdlib.h"
#include "stdio.h"


#define SIZE 3
#define transmit(msg) HAL_UART_Transmit_IT(&hlpuart1 , (uint8_t *) msg , strlen(msg));
#define CLOSE_DUTY 50 //5% of arr and hence to pass it to the prebuilt function * 10
#define OPEN_DUTY 75 //7.5% of arr , same as above
/**
 * Enumeration machine's states
 */
typedef enum fsm_state_enum{
ATTESA = 0, EROGAZIONE = 1 , ERRORE = 2
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
	//state of button
	button_state_t button;
	button_state_t button_last;

	//state of timer
	int timer_elapsed_first;
	int timer_elapsed;

	//code inserted state
	char codeInserted[SIZE];

}FSM_input_t;


/**
 * FSM Main Structure
 * It is composed by the input and outputs as well as
 * the current status of the machine and the current input reads
 */
typedef struct FSM_s
{
	//digital inputs
	button_t *p1 ;
	timer_t *timer;
	//digital outputs
	led_t *lg , *lr , *ly;
	//current state
	fsm_state_t state;
	//current input reads
	FSM_input_t in;
	//pwm
	timer_t *pwm;
	//uart
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


/*
 * Private function to update the current status and the output
 */
static int8_t FSM_update_state();


static int8_t FSM_stateAttesa();
static int8_t FSM_stateErrore();
static int8_t FSM_stateErogazione();



/*
 * Public init function
 */
int8_t FSM_init(button_t *p1 , timer_t *timer , timer_t *pwm , led_t *lg , led_t *lr , led_t *ly){

	if(!p1 || !timer || !pwm || !lg || !lr || !ly) return FSM_ERR;

	fsm.p1 = p1;
	fsm.timer = timer;
	fsm.pwm  = pwm;
	fsm.lg = lg ;
	fsm.lr = lr ;
	fsm.ly = ly ;

	fsm.state = ATTESA;

	if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	if(led_off(fsm.ly) != LED_OK) return FSM_ERR;
	if(led_on(fsm.lg) != LED_OK) return FSM_ERR;

	if(timer_set_period_ms(fsm.pwm,20) != TIMER_OK) return FSM_ERR;
	if(timer_set_duty_x10( fsm.pwm ,TIM_CHANNEL_1 ,CLOSE_DUTY) != TIMER_OK) return FSM_ERR;
	if(timer_start_pwm(fsm.pwm , TIM_CHANNEL_1) != TIMER_OK) return FSM_ERR;

	//trigger accumulation to form the code
	fsm.byteReady = 0;
	fsm.idx=0;
	fsm.codeReady=0;
	fsm.in.codeInserted[0] = '\0';
	transmit("Waiting for the user to insert the product code...\r\n");
	HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *) &fsm.byte , 1);

	return FSM_OK;
}
/*
 * Public step function
 */
int8_t FSM_step(){
	//version without cycle

	//read inputs
	if(FSM_read_inputs() != FSM_OK)return FSM_ERR;

	if(FSM_update_state() != FSM_OK) return FSM_ERR;

	return FSM_OK;

}

//********************************************************************************
//******	STATIC FUNCTIONS
//**************************************************************************

static int8_t FSM_read_inputs(){
	//save the last button state
	fsm.in.button_last = fsm.in.button;
	//read the new state of the button
	if(button_read(fsm.p1 , &fsm.in.button) != BUTTON_OK) return FSM_ERR;

	//verify whether timer is elapsed or not
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);


	if(fsm.state != ATTESA) return FSM_OK; //accumulate valid codes only when in proper state to prevent strange behaviors of the machine

	if(fsm.byteReady && !fsm.codeReady){
		fsm.byteReady = 0;

		//user pressed enter to terminate the string
		if((fsm.byte == '\r' || fsm.byte == '\n') && fsm.idx != 0){
			fsm.in.codeInserted[fsm.idx] = '\0';
			fsm.idx = 0;
			fsm.codeReady = 1;
		}

		else if (fsm.byte >= '0' && fsm.byte <= '9'){
			fsm.in.codeInserted[fsm.idx] = fsm.byte;
			fsm.idx ++;

			if(fsm.idx == SIZE - 1){
				fsm.in.codeInserted[fsm.idx] = '\0';
				fsm.idx = 0;
				fsm.codeReady = 1;

			}


	}



}
	return FSM_OK;
}


static int isValidCode(int code){
	return code >= 0 && code <=60 ;
}


static int8_t FSM_stateAttesa(){
	if(!fsm.codeReady) return FSM_OK; //continue waiting for the code to be inserted
	fsm.codeReady = 0; //code no more ready since we analyze it
	static char buf[40];

	int code = atoi(fsm.in.codeInserted);

	if (isValidCode(code)){
		fsm.state = EROGAZIONE;
		//entry actions:
		sprintf(buf , "Prodotto %d in erogazione \r\n" , code);
		transmit(buf);
		//move servo
		if(timer_set_duty_x10(fsm.pwm , TIM_CHANNEL_1 , OPEN_DUTY) != TIMER_OK) return FSM_ERR;
		//start timer of 10 seconds to keep track of correct erogation or invalid one
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period_ms(fsm.timer , 10000) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;

		if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
		if(led_on(fsm.ly) != LED_OK) return FSM_ERR;

	}
	else{
		//self loop into attesa
		fsm.state = ATTESA;
		fsm.byteReady =0;
		transmit("ATTESA SELEZIONE\r\n");
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
	}
return FSM_OK;
}


static int8_t FSM_stateErrore(){
	if(fsm.in.timer_elapsed){
		fsm.state = ATTESA;
		//entry actions:
		transmit("ATTESA SELEZIONE\r\n");
		if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		//trigger accumulation of another code
		fsm.byteReady = 0;
		HAL_UART_Receive_IT(&hlpuart1, (uint8_t *) &fsm.byte , 1);
	}

	return FSM_OK;
}


static int8_t FSM_stateErogazione(){
	if(fsm.in.button == GPIO_PIN_SET && fsm.in.button != fsm.in.button_last && !fsm.in.timer_elapsed){
		//erogation finished correctly
		fsm.state = ATTESA;
		//entry actions:
		transmit("ATTESA SELEZIONE\r\n");
		if(timer_set_duty_x10(fsm.pwm , TIM_CHANNEL_1 , CLOSE_DUTY) != TIMER_OK) return FSM_ERR;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
		if(led_off(fsm.ly) != LED_OK) return FSM_ERR;
		fsm.byteReady = 0;
		HAL_UART_Receive_IT(&hlpuart1, (uint8_t *) &fsm.byte , 1);
	}

	else if(fsm.in.timer_elapsed){
		fsm.state = ERRORE;
		//entry actions:
		transmit("ERRORE EROGAZIONE\r\n");
		if(timer_set_duty_x10(fsm.pwm , TIM_CHANNEL_1 , CLOSE_DUTY) != TIMER_OK) return FSM_ERR;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period_ms(fsm.timer , 5000) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_on(fsm.lr) != LED_OK) return FSM_ERR;
		if(led_off(fsm.ly) != LED_OK) return FSM_ERR;
	}
return FSM_OK;
}

static int8_t FSM_update_state(){

	switch(fsm.state){

	case ATTESA: return FSM_stateAttesa();
	case ERRORE: return FSM_stateErrore();
	case EROGAZIONE: return FSM_stateErogazione();
	default:
		return FSM_ERR;
		break;

	}

	return FSM_OK;
}







void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* handler) {
	if(handler == &htim6){
		timer_period_elapsed(fsm.timer , handler);
	}

}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.byteReady = 1;
		if(!fsm.codeReady) HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *) &fsm.byte , 1);
	}
}


