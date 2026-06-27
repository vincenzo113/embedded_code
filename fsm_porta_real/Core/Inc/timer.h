
#ifndef INC_TIMER_H_
#define INC_TIMER_H_
#include "stm32g4xx_hal.h"

#define TIMER_OK	(0)
#define TIMER_ERR	(-1)

struct timer_s
{
	TIM_HandleTypeDef* htim;
	uint8_t running;
	uint16_t elapsed;
	uint32_t in_frequency;
};

typedef struct timer_s timer_t;

/**
 * Function that initialize the timer
 */
int8_t timer_init(timer_t* timer, TIM_HandleTypeDef* handler, uint32_t frequency);

/*
 * Start the timer
 */
int8_t timer_start(timer_t* timer);

/*
 * Start the timer in pwm mode
 */
int8_t timer_start_pwm(timer_t* timer, uint16_t channel);


/*
 * Suspend the timer
 */
int8_t timer_stop(timer_t* timer);

/*
 * Suspend the timer and reset the counter
 */
int8_t timer_reset(timer_t* timer);

/*
 *
 * Check if the timer is running
 */
int8_t timer_is_running(timer_t* timer);

/*
 * Check if the period is elapsed
 */
int8_t timer_is_elapsed(timer_t* timer);

/**
 * Check the handler of the timer
 */
int8_t timer_is_myhandler(timer_t* timer, TIM_HandleTypeDef* handler);

/*
 * Setting the duty x10 1.2% -> 12
 */

int8_t timer_set_duty_x10(timer_t* timer, uint16_t channel, uint16_t duty_x10);
/*
 * Setting the period in millisecond of the update event
 */

int8_t timer_set_period_ms(timer_t* timer, uint16_t period);

/*
 * Setting the period in second of the update event
 */
int8_t timer_set_period(timer_t* timer, uint16_t period);

/*
 * Use in IT callback
 */
int8_t timer_period_elapsed(timer_t* timer, TIM_HandleTypeDef* handler);

#endif /* INC_TIMER_H_ */
