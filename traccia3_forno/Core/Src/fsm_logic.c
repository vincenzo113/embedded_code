#include "fsm_logic.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "tim.h"
#include "usart.h"

#define SIZE 3 // 2 plus the terminator char
#define print(msg) HAL_UART_Transmit_IT(&hlpuart1, (uint8_t *)(msg), strlen(msg))
char msg[100];

typedef enum fsm_state_enum {
  ATTESA = 0,
  COTTURA = 1,
  ERRORE_TEMPO = 2,
  ERRORE_SPORTELLO = 3
} fsm_state_t;

typedef struct FSM_input_s {
  button_state_t b1;
  button_state_t b1_last;
  button_state_t p1;
  button_state_t p1_last;
  uint8_t timer_elapsed;
  uint8_t timer_elapsed_first;

  char codeInserted[SIZE];

} FSM_input_t;

typedef struct FSM_s {
  button_t *b1, *p1;
  timer_t *timer;

  FSM_input_t in;

  led_t *lg, *ly, *lr;

  fsm_state_t state;

  int idx;
  char byte;
  int byteReady;
  int codeReady;

  uint32_t cycle;
} FSM_t;

static FSM_t fsm;

static int8_t FSM_read_inputs();
static int8_t FSM_update_state();
static int8_t FSM_stateAttesa();
static int8_t FSM_stateCottura();
static int8_t FSM_stateErroreTempo();
static int8_t FSM_stateErroreSportello();

int8_t FSM_init(button_t *b1, button_t *p1, timer_t *timer, led_t *lg,
                led_t *lr, led_t *ly) {
  if (b1 && p1 && timer && lg && lr && ly) {
    fsm.b1 = b1;
    fsm.p1 = p1;
    fsm.timer = timer;
    fsm.lg = lg;
    fsm.ly = ly;
    fsm.lr = lr;
  }

  if (button_set_delay(fsm.b1, 500) != BUTTON_OK)
    return FSM_ERR;
  if (button_set_delay(fsm.p1, 200) != BUTTON_OK)
    return FSM_ERR;

  fsm.state = ATTESA;

  if (led_off(fsm.lr) != LED_OK)
    return FSM_ERR;
  if (led_off(fsm.ly) != LED_OK)
    return FSM_ERR;
  if (led_on(fsm.lg) != LED_OK)
    return FSM_ERR;

  fsm.codeReady = 0;
  fsm.byteReady = 0;
  fsm.idx = 0;
  print("UNSERIRE DURATA...\r\n");
  HAL_UART_Receive_IT(&hlpuart1, (uint8_t *)&fsm.byte, 1);

  return FSM_OK;
}

int8_t FSM_step() {
  if (FSM_read_inputs() != FSM_OK)
    return FSM_ERR;
  if (FSM_update_state() != FSM_OK)
    return FSM_ERR;
  return FSM_OK;
}

static int8_t FSM_read_inputs() {
  fsm.in.b1_last = fsm.in.b1;
  fsm.in.p1_last = fsm.in.p1;
  if (button_read(fsm.b1, &fsm.in.b1) != BUTTON_OK)
    return FSM_ERR;
  if (button_read(fsm.p1, &fsm.in.p1) != BUTTON_OK)
    return FSM_ERR;

  fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);

  //if in wait state and byte is ready and code is not ready continue to accumulate
  if (fsm.state == ATTESA && fsm.byteReady && !fsm.codeReady) {

    fsm.byteReady = 0;

    if ((fsm.byte == '\r' || fsm.byte == '\n') && (fsm.idx != 0)) {
      fsm.in.codeInserted[fsm.idx] = '\0';
      fsm.idx = 0;
      fsm.codeReady = 1;
    }
    else if (fsm.byte >= '0' && fsm.byte <= '9') {
      fsm.in.codeInserted[fsm.idx] = fsm.byte;
      fsm.idx++;
      if (fsm.idx == SIZE - 1) {
        fsm.in.codeInserted[fsm.idx] = '\0';
        fsm.idx = 0;
        fsm.codeReady = 1;
      }
    }
  }

  return FSM_OK;
}

static int8_t FSM_update_state() {
  switch (fsm.state) {
  case ATTESA: {
    if (FSM_stateAttesa() != FSM_OK)
      return FSM_ERR;
    break;
  }
  case COTTURA: {
    if (FSM_stateCottura() != FSM_OK)
      return FSM_ERR;
    break;
  }
  case ERRORE_TEMPO: {
    if (FSM_stateErroreTempo() != FSM_OK)
      return FSM_ERR;
    break;
  }
  case ERRORE_SPORTELLO: {
    if (FSM_stateErroreSportello() != FSM_OK)
      return FSM_ERR;
    break;
  }
  default:
    return FSM_ERR;
  }
  return FSM_OK;
}

static int8_t FSM_stateAttesa() {

  if (fsm.codeReady) {


      int codice = atoi(fsm.in.codeInserted);

      if (codice > 0 && codice <= 60) {
        if (fsm.in.p1 == GPIO_PIN_SET) {
          fsm.state = COTTURA;
          sprintf(msg, "PRODOTTO IN COTTURA PER %d SECONDI\r\n", codice);
          print(msg);
          if (timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
          if (timer_set_period(fsm.timer, codice) != TIMER_OK) return FSM_ERR;
          if (timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
          if (led_off(fsm.lg) != LED_OK) return FSM_ERR;
          if (led_on(fsm.ly) != LED_OK) return FSM_ERR;
        } else {
          fsm.state = ERRORE_SPORTELLO;
          print("CHIUDERE LO SPORTELLO\r\n");
          if (led_off(fsm.lg) != LED_OK) return FSM_ERR;
          if (led_on(fsm.lr) != LED_OK) return FSM_ERR;
        }
      } else {
        print("CODICE NON VALIDO -> ERRORE_TEMPO\r\n");
        fsm.state = ERRORE_TEMPO;
        if (timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
        if (timer_set_period(fsm.timer, 3) != TIMER_OK) return FSM_ERR;
        if (timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
        if (led_off(fsm.lg) != LED_OK) return FSM_ERR;
        if (led_on(fsm.lr) != LED_OK) return FSM_ERR;
      }
    }

  return FSM_OK;
  }



static int8_t FSM_stateCottura() {
  if (fsm.in.timer_elapsed ||
      (fsm.in.b1 == GPIO_PIN_SET && fsm.in.b1 != fsm.in.b1_last) ||
      (fsm.in.p1 == GPIO_PIN_SET && fsm.in.p1 != fsm.in.p1_last)) {
    fsm.state = ATTESA;
    fsm.codeReady = 0;
    fsm.idx = 0;
    fsm.byteReady = 0;
    print("INSERIRE DURATA...\r\n");
    if (timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
    if (led_off(fsm.ly) != LED_OK) return FSM_ERR;
    if (led_on(fsm.lg) != LED_OK) return FSM_ERR;
    HAL_UART_Receive_IT(&hlpuart1, (uint8_t *)&fsm.byte, 1);
  }
  return FSM_OK;
}

static int8_t FSM_stateErroreTempo() {
  if (fsm.in.timer_elapsed) {
    fsm.state = ATTESA;
    fsm.codeReady = 0;
    fsm.byteReady = 0;
    fsm.idx = 0;
    print("INSERIRE DURATA...\r\n");
    HAL_UART_Receive_IT(&hlpuart1, (uint8_t *)&fsm.byte, 1);
    if (timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
    if (led_off(fsm.lr) != LED_OK) return FSM_ERR;
    if (led_on(fsm.lg) != LED_OK) return FSM_ERR;
  }
  return FSM_OK;
}

static int8_t FSM_stateErroreSportello() {
  if (fsm.in.p1 == GPIO_PIN_SET && fsm.in.p1 != fsm.in.p1_last) {
    fsm.state = COTTURA;
    sprintf(msg, "PRODOTTO IN COTTURA PER %d SECONDI\r\n",
            atoi(fsm.in.codeInserted));
    print(msg);
    if (timer_reset(fsm.timer) != TIMER_OK) return FSM_ERR;
    if (timer_set_period(fsm.timer, atoi(fsm.in.codeInserted)) != TIMER_OK) return FSM_ERR;
    if (timer_start(fsm.timer) != TIMER_OK) return FSM_ERR;
    if (led_off(fsm.lr) != LED_OK) return FSM_ERR;
    if (led_on(fsm.ly) != LED_OK) return FSM_ERR;
  }
  return FSM_OK;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart == &hlpuart1) {
    fsm.byteReady = 1;
    if (!fsm.codeReady)
      HAL_UART_Receive_IT(&hlpuart1, (uint8_t *)&fsm.byte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart == &hlpuart1) {
    HAL_UART_AbortReceive(huart);
    HAL_UART_Receive_IT(&hlpuart1, (uint8_t *)&fsm.byte, 1);
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *handler) {
  if (handler == &htim6) {
    timer_period_elapsed(fsm.timer, handler);
  }
}
