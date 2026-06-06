#include "fsm_logic.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "usart.h"
#include "tim.h"
#define SIZE 3
#define ZERO_GRAD 2
#define NOVANTA_GRAD 3
#define CENTOTTANTA_GRAD 4

char var [60]; //where to store the output of the sprintf command


/**
 * Enumeration machine's states
 */
typedef enum fsm_state_enum{
	ATTESA= 0,
	EROGAZIONE= 1,
	ERRORE= 2
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
}FSM_input_t;


/**
 * FSM Main Structure
 * It is composed by the input and outputs as well as
 * the current status of the machine and the current input reads
 */
typedef struct FSM_s
{
	// FSM Inputs devices
	button_t *p1; //sensor device
	timer_t *timer; //timer to keep track of transition to do based on time elapsing

	// FSM Inputs state
	FSM_input_t in;

	// FSM Outputs devices
	led_t *lg, *ly, *lr;

	// FSM current status
	fsm_state_t state;

	//Cycle duration in ms 
	uint32_t cycle;

	//UART section
	uint8_t byte; //where to store each char to read
	uint8_t uartBusy; //to see if uart is busy and hence to avoid buffer overrun
	uint8_t buffer[SIZE]; //buffer where to store each code inserted after having processed it
	uint8_t idx;
	uint8_t codeReady; //to signal that code is ready when we end reading from uart
	///
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
static int8_t FSM_stateErogazione();
static int8_t FSM_stateErrore();


/***
 * Utility functions
 *
 */
void printMsg(const char *msg){
	if(!fsm.uartBusy){
	HAL_UART_Transmit_IT(&hlpuart1 , (uint8_t *)msg , strlen(msg));
	fsm.uartBusy =1;
	}
}



/*
 * Public init function
 */
int8_t FSM_init(uint32_t cycle , led_t* lr, led_t* lg,led_t* ly, button_t* p1,timer_t* timer){
	if(lr && lg && ly && p1 && timer){
		fsm.lr=lr;
		fsm.lg=lg;
		fsm.ly=ly;
		fsm.p1=p1;
		fsm.timer=timer;
		fsm.cycle = cycle;
	}

	//initialize state and peripherals
	__HAL_TIM_SET_COMPARE(&htim4 ,TIM_CHANNEL_1 , ZERO_GRAD); //0 gradi initially
	HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_1);


	//initial state is wait
	fsm.state = ATTESA;
	fsm.codeReady = 0;
	fsm.uartBusy = 0;
	//if uart is not busy transmit

		printMsg("ATTESA SELEZIONE... \r\n");


	//turn on led green
	if(led_on(fsm.lg) != LED_OK) return FSM_ERR;

	//start receiving for data as specified in the document
	if(!fsm.uartBusy){
		HAL_UART_Receive_IT(&hlpuart1,&fsm.byte,1); //this is not required since receive is not an entry action for state ATTESA though it is a an action to perform each cycle if we are in that state
		fsm.uartBusy = 1;
	}

return FSM_OK;



}

/*
 * Public step function
 */
int8_t FSM_step(){
uint8_t start = HAL_GetTick();
if(FSM_read_inputs() != FSM_OK) return FSM_ERR;
if(FSM_update_state() != FSM_OK) return FSM_ERR;
uint8_t elapsedTime = start - HAL_GetTick();
if(elapsedTime > fsm.cycle) return FSM_ERR;
else HAL_Delay(fsm.cycle - elapsedTime);

return FSM_OK;
}

//********************************************************************************
//******	STATIC FUNCTIONS
//**************************************************************************

static int8_t FSM_read_inputs(){
	fsm.in.button_last = fsm.in.button;
	if(button_read(fsm.p1 , &fsm.in.button) != BUTTON_OK) return FSM_ERR;
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);
	return FSM_OK;
}


static int8_t FSM_update_state(){
	switch(fsm.state){
	case ATTESA:
		{
			FSM_stateAttesa();
			break;
		}
	case EROGAZIONE:
		{
			FSM_stateErogazione();
			break;
		}
	case ERRORE:
		{
			FSM_stateErrore();
			break;
		}
	}

	return FSM_OK;
}


static int8_t FSM_stateAttesa(){

	//code is not ready , so ask for another receive
	if(!fsm.codeReady && !fsm.uartBusy){
		HAL_UART_Receive_IT(&hlpuart1,&fsm.byte,1);
	}
	//code is ready
	else if(fsm.codeReady){
		fsm.codeReady = 0; //code is no more ready

		uint8_t code = atoi(fsm.buffer);

		//code is correct
		if(code >= 0 && code <= 60){
			fsm.state =  EROGAZIONE; //enter state erogation
			//entry actions:

			__HAL_TIM_SET_COMPARE(&htim4 ,TIM_CHANNEL_1 , NOVANTA_GRAD); //move the servo to 90
			if(led_off(fsm.lg) != LED_OK) return FSM_ERR; //turn off green led
			if(led_on(fsm.ly) != LED_OK) return FSM_ERR; //turn on yellow led
			timer_set_period(fsm.timer,10000); //start timer for 10 seconds
			timer_start(fsm.timer);
			sprintf(var , "Prodotto %d IN EROGAZIONE..." , code);
			printMsg(var);

		}
		//code is not correct
		else
			{
				fsm.idx = 0;
				if(!fsm.uartBusy) HAL_UART_Receive_IT(&hlpuart1,&fsm.byte,1);

			}

	}

return FSM_OK;
}

static int8_t FSM_stateErogazione(){
	//if button is pressed before timer to elapse erogation is done correctly
	if(fsm.in.button == GPIO_PIN_SET && !fsm.in.timer_elapsed && fsm.in.button != fsm.in.button_last){
		printMsg("Prodotto erogato correttamente , attesa selezione...\r\n");
		//ENTRY ACTIONS FOR STATE ATTESA:
		if(led_off(fsm.ly) != LED_OK)return FSM_ERR;
		if(led_on(fsm.lg) != LED_OK) return FSM_ERR;

		__HAL_TIM_SET_COMPARE(&htim4 ,TIM_CHANNEL_1 , ZERO_GRAD); //move the servo to 0

		fsm.state = ATTESA;
	}
	else if(fsm.in.timer_elapsed){
		fsm.state = ERRORE;
		printMsg("Errore di erogazione...\r\n");
		if(timer_stop(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period(fsm.timer , 5000) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(led_off(fsm.ly) != LED_OK)return FSM_ERR;
		if(led_on(fsm.lr) != LED_OK) return FSM_ERR;
	}
return FSM_OK;
}




static int8_t FSM_stateErrore(){
	if(fsm.in.timer_elapsed){
		fsm.state = ATTESA;
		//entry actions
		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
		if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
		printMsg("ATTESA SELEZIONE...\r\n"); //cause we are entering wait state and hence it is the entry action for wait state
		__HAL_TIM_SET_COMPARE(&htim4 ,TIM_CHANNEL_1 , ZERO_GRAD); //move the servo to 0
	}
	return FSM_OK;
}




void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* handler) {
	if(handler == &htim6){
		timer_period_elapsed(fsm.timer , handler);

	}
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.uartBusy = 0;
	}
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.uartBusy = 0;

		char byte = fsm.byte; //save byte received

		//byte received is enter
		if( ((byte == '\r') || (byte == '\n')) && fsm.idx != 0){
			fsm.buffer[fsm.idx] = '\0';
			fsm.codeReady=1;
			fsm.idx = 0; //resets for next digits
		}
		else{
			if(byte >= '0' && byte <= '9'){
				fsm.buffer[fsm.idx] = byte;
				fsm.idx = fsm.idx+1;
				//check if full
				if(fsm.idx == SIZE-1){
					fsm.buffer[fsm.idx] = '\0';
					fsm.codeReady = 1;
					fsm.idx = 0;
				}
			}
		}



	}
}

