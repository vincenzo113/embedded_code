#include "fsm_logic.h"
#define SIZE 3
#include "usart.h"
#include "string.h"
#include "tim.h"
#include "stdlib.h"
#include "stdio.h"


#define ATTESA_MSG "ATTESA SELEZIONE...\r\n"
#define ERRORE_MSG "ERRORE EROGAZIONE...\r\n"
#define print(msg) HAL_UART_Transmit_IT(&hlpuart1, (uint8_t *) msg , strlen(msg));

char erogazioneMsg[50];

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
	button_state_t p1;	 		//Stable value of the button at current step
	button_state_t p1_last;		//Stable value of the button at previous step
	uint8_t timer_elapsed;
	uint8_t timer_elapsed_first;		//Used to avoid the first timer interrupt


	//status code
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
	button_t *p1;
	timer_t *timer;

	// FSM Inputs state
	FSM_input_t in;

	// FSM Outputs devices
	led_t *ly, *lg, *lr;

	// FSM current status
	fsm_state_t state;

	//Cycle duration in ms 
	uint32_t cycle;


	//uart things
	int idx;
	char byte;
	int codeReady;
	int byteReady;

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



/*
 * Public init function
 */
int8_t FSM_init(led_t* lg, led_t* lr, led_t* ly,  button_t* p1 , timer_t* timer){
	if(lg && lr && ly && p1 && timer){
		fsm.lg = lg ;
		fsm.ly = ly ;
		fsm.lr = lr;
		fsm.p1 = p1;
		fsm.timer = timer;
	}

	fsm.in.codeInserted[0] = '\0';
	fsm.byteReady = 0;
	fsm.codeReady = 0;
	fsm.idx = 0;

	if(button_set_delay(fsm.p1 , 500) != BUTTON_OK) return FSM_ERR;

	fsm.state = ATTESA;
	//entry actions for state attesa:
	if(led_on(fsm.lg) != LED_OK) return FSM_ERR;
	print(ATTESA_MSG);
	HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1); //trigger accumulation


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

//********************************************************************************
//******	STATIC FUNCTIONS
//**************************************************************************

static int8_t FSM_read_inputs(){
	fsm.in.p1_last = fsm.in.p1;
	if(button_read(fsm.p1 , &fsm.in.p1) != BUTTON_OK) return FSM_ERR;
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);


	//make accumulation phase of chars only if we are in ATTESA
	if(fsm.state != ATTESA) return FSM_OK;

	if(fsm.byteReady && !fsm.codeReady){
		fsm.byteReady = 0;
		//process the byte received through uart
		if((fsm.byte == '\r' || fsm.byte == '\n') && fsm.idx != 0){
			//user pressed enter
			fsm.in.codeInserted[fsm.idx] = '\0';
			fsm.idx = 0;
			fsm.codeReady=1;

		}

		else if(fsm.byte >= '0' && fsm.byte <= '9'){
			fsm.in.codeInserted[fsm.idx] = fsm.byte;
			fsm.idx ++;
			if(fsm.idx == SIZE-1){
				fsm.in.codeInserted[fsm.idx] = '\0';
				fsm.codeReady =1;
				fsm.idx = 0;
			}
		}


	}

	return FSM_OK;
}


static int8_t FSM_update_state(){
	switch(fsm.state){
	case ATTESA:{
		if(FSM_stateAttesa() != FSM_OK) return FSM_ERR;
		break;
	}
	case EROGAZIONE: {
		if(FSM_stateErogazione() != FSM_OK) return FSM_ERR;
		break;
	}

	case ERRORE: {
		if(FSM_stateErrore()  != FSM_OK) return FSM_ERR;
		break;
	}
	default: return FSM_ERR;

}
	return FSM_OK;
}


static int8_t FSM_stateAttesa(){

	//if code is not ready wait for it to be ready
	if(!fsm.codeReady) return FSM_OK;


	//process code received
	int codeInserted = atoi(fsm.in.codeInserted);

	fsm.codeReady = 0;

	if(codeInserted >= 0 && codeInserted <= 60){
		sprintf(erogazioneMsg , "PRODOTTO %d IN EROGAZIONE...\r\n" , codeInserted);
		fsm.state = EROGAZIONE;

		//entry actions for EROGAZIONE STATE:
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period(fsm.timer , 10) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
		print(erogazioneMsg);


	}
	else{
		fsm.state = ATTESA;
		fsm.byteReady = 0;
		print("INSERIRE UN VALORE VALIDO...\r\n");
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);

	}



	return FSM_OK;
}

static int8_t FSM_stateErogazione(){

	if(fsm.in.p1 == GPIO_PIN_SET && fsm.in.p1 != fsm.in.p1_last && !fsm.in.timer_elapsed){
		//prodotto erogato correttamente
		print("PRODOTTO EROGATO CORRETTAMENTE\r\n");
		fsm.state = ATTESA;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		fsm.byteReady = 0;
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
	}

	else if (fsm.in.timer_elapsed){
		print(ERRORE_MSG);
		fsm.state = ERRORE;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		if(timer_set_period(fsm.timer , 5) != TIMER_OK) return FSM_ERR;
		if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
	}



	return FSM_OK;
}

static int8_t FSM_stateErrore(){

	if(fsm.in.timer_elapsed){
		fsm.state = ATTESA;
		if(timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
		fsm.byteReady = 0;
		print(ATTESA_MSG);
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
	}

	return FSM_OK;
}





void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.byteReady = 1;

		if(!fsm.codeReady) HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
	}
}


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		HAL_UART_AbortReceive(huart);
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
	}
}




void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* handler) {
	if(handler == &htim6){
		timer_period_elapsed(fsm.timer , handler);
	}
}


