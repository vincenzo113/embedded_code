#ifndef INC_FSM_LOGIC_H_
#define INC_FSM_LOGIC_H_

#include "led.h"
#include "button.h"
#include "timer.h"
#include "queue.h"
#include "usart.h"
#include "tim.h"

#define FSM_ERR		(-1)
#define FSM_OK		(0)

#define FSM_CYCLE_DURATION		(100) //milliseconds

/**
 * Public function to initialize the FMS
 */
int8_t FSM_init(button_t *button , timer_t *timer , led_t *lg , led_t *lr , timer_t *timer_pwm);
/**
 * Public function to be called to evolve the FSM Status
 * This function allows to read the inputs and update the current status and the output
 * The function must be called iteratively after the machine has been initialized
 */
int8_t FSM_step();


#endif /* INC_FSM_LOGIC_H_ */
