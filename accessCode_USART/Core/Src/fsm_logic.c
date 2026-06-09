#include "fsm_logic.h"
#define SIZE 3 //2 plus the terminator char
#include "stdlib.h"




/**
 * Enumeration machine's states
 */
typedef enum fsm_state_enum{
	RICEZIONE= 0,
	ACCESSO_GARANTITO= 1,
	ACCESSO_NEGATO = 2
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
	uint8_t timer_elapsed;
	uint8_t timer_elapsed_first;		//Used to avoid the first timer interrupt
	char passInserted[SIZE];
}FSM_input_t;


/**
 * FSM Main Structure
 * It is composed by the input and outputs as well as
 * the current status of the machine and the current input reads
 */
typedef struct FSM_s
{
	// FSM Inputs devices
	timer_t *timer;

	// FSM Inputs state
	FSM_input_t in;

	// FSM Outputs devices
	led_t *lg, *lr;

	// FSM current status
	fsm_state_t state;

	//Cycle duration in ms 
	uint32_t cycle;

	//uart things
	uint8_t codeReady;
	char byte;
	uint8_t byteReady;
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
static int8_t FSM_stateAccessoGarantito();
static int8_t FSM_stateAccessoNegato();



/*
 * Public init function
 */
int8_t FSM_init(uint32_t cycle , led_t* lg, led_t* lr  , timer_t* timer){
	if(cycle && lg && lr &&timer){
		fsm.cycle = cycle;
		fsm.lg = lg;
		fsm.lr = lr;
		fsm.timer = timer;
	}


	//all led off
	if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
	if(led_off(fsm.lr) != LED_OK) return FSM_ERR;
	//no code is ready , neither no digit has been inserted
	fsm.idx=0;
	fsm.codeReady = 0;
	fsm.byteReady = 0;
	//initial state is ricezione
	fsm.state = RICEZIONE;
	//entry actions for RICEZIONE state:
	HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1); //start accumulation phase
	return FSM_OK;
}

/*
 * Public step function
 */
int8_t FSM_step(){
	uint32_t start = HAL_GetTick();
	if(FSM_read_inputs() != FSM_OK) return FSM_ERR;
	if(FSM_update_state() != FSM_OK) return FSM_ERR;

	uint32_t elapsedTime = HAL_GetTick() -start;
	if(elapsedTime > fsm.cycle) return FSM_ERR;
	else HAL_Delay(fsm.cycle - elapsedTime);
	return FSM_OK;
}

//********************************************************************************
//******	STATIC FUNCTIONS
//**************************************************************************

static int8_t FSM_read_inputs(){
	//read timer status
	fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);

	//only if we are in RICEZIONE state we must accumulate chars
	if(fsm.state == RICEZIONE){

		if(fsm.byteReady){
			fsm.byteReady = 0; //byte is no more available after processing it

			//process the byte received
			HAL_UART_Transmit(&hlpuart1, (uint8_t *)&fsm.byte, 1, 100);


			//USER PRESSED ENTER
			if((fsm.byte == '\r'|| fsm.byte == '\n') && fsm.idx!=0){
				fsm.in.passInserted[fsm.idx] = '\0';
				fsm.idx = 0;
				fsm.codeReady = 1;
			}
			//VALID BYTE INSERTED
			else if (fsm.byte >= '0' && fsm.byte <= '9'){
				fsm.in.passInserted[fsm.idx] = fsm.byte;
				fsm.idx++;
				if(fsm.idx == SIZE-1){
					fsm.in.passInserted[fsm.idx] = '\0';
					fsm.codeReady = 1;
					fsm.idx = 0;
				    HAL_UART_Transmit(&hlpuart1, (uint8_t *)"READY\r\n", 7, 100); // DEBUG

				}

				if(!fsm.codeReady) HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);

			}
			else //covers situation where we read when idx = 0 a \n and hence we could lose receiveness if no enabling again
				HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
		}




	}

	return FSM_OK;
}





static int8_t FSM_stateRicezione();
static int8_t FSM_stateErrore();
static int8_t FSM_stateAccessoGarantito();

static int8_t FSM_update_state(){
	switch(fsm.state){
	case RICEZIONE:
		{
		if(FSM_stateRicezione() != FSM_OK) return FSM_ERR;
		break;
		}

	case ACCESSO_GARANTITO: {
		if(FSM_stateAccessoGarantito() != FSM_OK) return FSM_ERR;
		break;
	}
	case ACCESSO_NEGATO: {
		if(FSM_stateErrore() != FSM_OK) return FSM_ERR;
		break;
	}
	}
	return FSM_OK;
}


static int8_t FSM_stateRicezione(){
	if(fsm.codeReady){


		   // DEBUG: stampa cosa hai ricevuto
		        HAL_UART_Transmit(&hlpuart1, (uint8_t *)"CODE:[", 6, 100);
		        HAL_UART_Transmit(&hlpuart1, (uint8_t *)fsm.in.passInserted, SIZE, 100);
		        HAL_UART_Transmit(&hlpuart1, (uint8_t *)"]\r\n", 3, 100);
		fsm.codeReady = 0; //reset the flag since code is not ready anymore
		int numeric_code = atoi(fsm.in.passInserted);
		if(numeric_code == 12){
			fsm.state = ACCESSO_GARANTITO;
			//entry actions for this state:
			if(timer_stop(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(timer_set_period(fsm.timer, 2) != TIMER_OK) return FSM_ERR;
			if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(led_on(fsm.lg) != LED_OK) return FSM_ERR;


		}
		else{
			//transition to the error state
			fsm.state = ACCESSO_NEGATO;

			//entry actions for this state:
			if(timer_stop(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(timer_set_period(fsm.timer, 2) != TIMER_OK) return FSM_ERR;
			if(timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
			if(led_on(fsm.lr) != LED_OK) return FSM_ERR;
		}

	}


	return FSM_OK;
}

static int8_t FSM_stateAccessoGarantito(){

	if(fsm.in.timer_elapsed){
		//go back to attesa
		fsm.state = RICEZIONE;

		//entry actions for this state
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);
		if(led_off(fsm.lg) != LED_OK) return FSM_ERR;
		if(timer_stop(fsm.timer) != TIMER_OK) return FSM_ERR;
		return FSM_OK;

	}






	return FSM_OK;
}


static int8_t FSM_stateErrore(){
	//go back to attesa
	if(fsm.in.timer_elapsed){
		fsm.state = RICEZIONE;

		//entry actions for this state
		HAL_UART_Receive_IT(&hlpuart1 , (uint8_t *)&fsm.byte , 1);

		if(led_off(fsm.lr) != LED_OK) return FSM_ERR;

		if(timer_stop(fsm.timer) != TIMER_OK) return FSM_ERR;
	}

	return FSM_OK;
}








void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* handler) {

	if(handler == &htim6){
		timer_period_elapsed(fsm.timer, handler);
	}
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		fsm.byteReady = 1; //a byte has been receipted correctly
	}
}


//utile per evitare situazione di buffer overrun che possono accadere quando l'utente digita in stati non permessi per questa operazione e quindi occorre sempre mantenerla pulita
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){
	if(huart == &hlpuart1){
		// Se c'è stato un errore (es. Overrun), riavvia la ricezione
		HAL_UART_AbortReceive(huart); //prima pulisci i dati ricevuti non durante lo stato di ricezione
		HAL_UART_Receive_IT(&hlpuart1, (uint8_t *)&fsm.byte, 1); //riabiita la ricezione in maniera tale che possiamo essere sempre in ascolto sull'uart
	}
}




