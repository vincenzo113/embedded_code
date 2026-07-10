#include "fsm_logic.h"
#include "ds3231_for_stm32_hal.h"
#include "string.h"
#include "stdio.h"

#define DEFAULT_BUTTON_DELAY    (3000)
#define INACTIVE_BLINK_HPERIOD  (500)
#define RED_DURATION            (10)
#define YELLOW_DURATION         (15)
#define GREEN_DURATION          (10)

extern UART_HandleTypeDef hlpuart1;
extern I2C_HandleTypeDef hi2c1;

typedef enum fsm_state_enum {
    STAND_BY  = 0,
    RED_ON    = 1,
    YELLOW_ON = 2,
    GREEN_ON  = 3
} fsm_state_t;

typedef struct FSM_input_s {
    button_state_t alarm;
    button_state_t time_req;
    uint8_t        timer_elapsed;
} FSM_input_t;

typedef struct FSM_s {
    button_t*   alarm;
    button_t*   time_req;
    timer_t*    timer;
    FSM_input_t in;
    led_t*      led[3];
    fsm_state_t state;
    uint8_t     uart_tx_completed;
} FSM_t;

static FSM_t fsm;

static int8_t FSM_read_inputs();
static int8_t FSM_update_state();
static int8_t FSM_latern_inactive();
static int8_t FSM_latern_red();
static int8_t FSM_latern_yellow();
static int8_t FSM_latern_green();

int8_t FSM_init(led_t* led_1, led_t* led_2, led_t* led_3, button_t* alarm_interrupt, button_t* timestamp_req, timer_t* timer) {
    if (!led_1 || !led_2 || !led_3 || !alarm_interrupt || !timestamp_req || !timer)
        return FSM_ERR;

    fsm.state  = STAND_BY;
    fsm.led[0] = led_1;
    fsm.led[1] = led_2;
    fsm.led[2] = led_3;
    fsm.alarm    = alarm_interrupt;
    fsm.time_req = timestamp_req;
    fsm.timer    = timer;
    fsm.uart_tx_completed = 1;

    if (button_set_delay(fsm.alarm, DEFAULT_BUTTON_DELAY) != BUTTON_OK)
        return FSM_ERR;
    if (button_set_delay(fsm.time_req, DEFAULT_BUTTON_DELAY) != BUTTON_OK)
        return FSM_ERR;
    if (led_set_toggle_period(fsm.led[1], INACTIVE_BLINK_HPERIOD) != LED_OK)
        return FSM_ERR;

    print_message("FSM inizializzata - attualmente in stand-by.\r\n");
    return FSM_OK;
}

int8_t FSM_step() {
    if (FSM_read_inputs() != FSM_OK)
        return FSM_ERR;

    return FSM_update_state();
}

static int8_t FSM_read_inputs() {

	uint8_t res = FSM_OK;
	/*
    if (button_get_and_clear_pressed(fsm.alarm, &fsm.in.alarm) != BUTTON_OK)
        return FSM_ERR;

    if (button_get_and_clear_pressed(fsm.time_req, &fsm.in.time_req) != BUTTON_OK)
        return FSM_ERR;*/

	if (button_read_pressed(fsm.alarm, &fsm.in.alarm) != BUTTON_OK ||
		button_reset_pressed(fsm.alarm) != BUTTON_OK) {
			res = FSM_ERR;
	}

	if (button_read_pressed(fsm.time_req, &fsm.in.time_req) != BUTTON_OK ||
		button_reset_pressed(fsm.time_req) != BUTTON_OK) {
			res = FSM_ERR;
	}


    fsm.in.timer_elapsed = timer_is_elapsed(fsm.timer);
    return res;
}

static int8_t FSM_update_state() {
    switch (fsm.state) {
        case STAND_BY:  return FSM_latern_inactive();
        case RED_ON:    return FSM_latern_red();
        case YELLOW_ON: return FSM_latern_yellow();
        case GREEN_ON:  return FSM_latern_green();
        default:        return FSM_ERR;
    }
}

static int8_t FSM_latern_inactive() {
    if (led_toggle(fsm.led[1]) != LED_OK)
        return FSM_ERR;

    if (fsm.in.alarm != PRESSED)
        return FSM_OK;

    if (led_on(fsm.led[2]) != LED_OK || led_off(fsm.led[1]) != LED_OK)
        return FSM_ERR;

    fsm.state = GREEN_ON;
    print_message("Lanterna impostata su VERDE.\r\n");

    if (timer_reset(fsm.timer) != TIMER_OK ||
        timer_set_period(fsm.timer, GREEN_DURATION) != TIMER_OK ||
        timer_start(fsm.timer) != TIMER_OK)
        return FSM_ERR;

    return FSM_OK;
}

static int8_t FSM_latern_red() {
    if (fsm.in.alarm == PRESSED) {
        if (led_off(fsm.led[0]) != LED_OK || timer_reset(fsm.timer) != TIMER_OK)
            return FSM_ERR;
        fsm.state = STAND_BY;
        print_message("Lanterna impostata in stand-by.\r\n");
        return FSM_OK;
    }

    if (fsm.in.timer_elapsed) {
        if (led_off(fsm.led[0]) != LED_OK || led_on(fsm.led[2]) != LED_OK)
            return FSM_ERR;
        fsm.state = GREEN_ON;
        print_message("Lanterna impostata su VERDE.\r\n");
        if (timer_reset(fsm.timer) != TIMER_OK ||
            timer_set_period(fsm.timer, GREEN_DURATION) != TIMER_OK ||
            timer_start(fsm.timer) != TIMER_OK)
            return FSM_ERR;
    }

    return FSM_OK;
}

static int8_t FSM_latern_yellow() {
    if (fsm.in.alarm == PRESSED) {
        if (led_off(fsm.led[1]) != LED_OK || timer_reset(fsm.timer) != TIMER_OK)
            return FSM_ERR;
        fsm.state = STAND_BY;
        print_message("Lanterna impostata in stand-by.\r\n");
        return FSM_OK;
    }

    if (fsm.in.timer_elapsed) {
        if (led_off(fsm.led[1]) != LED_OK || led_on(fsm.led[0]) != LED_OK)
            return FSM_ERR;
        fsm.state = RED_ON;
        print_message("Lanterna impostata su ROSSO.\r\n");
        if (timer_reset(fsm.timer) != TIMER_OK ||
            timer_set_period(fsm.timer, RED_DURATION) != TIMER_OK ||
            timer_start(fsm.timer) != TIMER_OK)
            return FSM_ERR;
    }

    return FSM_OK;
}

static int8_t FSM_latern_green() {
    if (fsm.in.alarm == PRESSED) {
        if (led_off(fsm.led[2]) != LED_OK || timer_reset(fsm.timer) != TIMER_OK)
            return FSM_ERR;
        fsm.state = STAND_BY;
        print_message("Lanterna impostata in stand-by.\r\n");
        return FSM_OK;
    }

    if (fsm.in.timer_elapsed) {
        if (led_off(fsm.led[2]) != LED_OK || led_on(fsm.led[1]) != LED_OK)
            return FSM_ERR;
        fsm.state = YELLOW_ON;
        print_message("Lanterna impostata su GIALLO.\r\n");
        if (timer_reset(fsm.timer) != TIMER_OK ||
            timer_set_period(fsm.timer, YELLOW_DURATION) != TIMER_OK ||
            timer_start(fsm.timer) != TIMER_OK)
            return FSM_ERR;
    }

    return FSM_OK;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

    if (GPIO_Pin == RTC_IT_Pin) {
    	if (DS3231_IsAlarm1Triggered()) {
    		DS3231_ClearAlarm1Flag();
    		print_message("Interrupt ALARM 1.\r\n");
    	} else if (DS3231_IsAlarm2Triggered()) {
    		DS3231_ClearAlarm2Flag();
    		print_message("Interrupt ALARM 2.\r\n");
    	}

    	print_message("\r\n");
        button_pressed_handler(fsm.alarm, GPIO_Pin);
    }
    if (GPIO_Pin == Time_REQ_Pin) {
    	print_message("Richiesta timestamp utente.\r\n");
        button_pressed_handler(fsm.time_req, GPIO_Pin);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* handler) {
    timer_elapsed_handler(fsm.timer, handler);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == LPUART1)
        fsm.uart_tx_completed = 1;
}

void print_message(char* msg) {
    static char tx_buffer[96];
    snprintf(tx_buffer, sizeof(tx_buffer),
        "[%04u-%02u-%02u %02u:%02u:%02u] %s",
        DS3231_GetYear(), DS3231_GetMonth(), DS3231_GetDate(),
        DS3231_GetHour(), DS3231_GetMinute(), DS3231_GetSecond(),
        msg);
    if (HAL_UART_Transmit_IT(&hlpuart1, (uint8_t *)tx_buffer, strlen(tx_buffer) + 1) == HAL_OK)
        fsm.uart_tx_completed = 0;
}
