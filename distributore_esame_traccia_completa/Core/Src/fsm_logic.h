#ifndef INC_FSM_LOGIC_H_
#define INC_FSM_LOGIC_H_

#include "led.h"
#include "button.h"
#include "timer.h"


#define FSM_ERR		(-1)
#define FSM_OK		(0)

/**
 * Public function to initialize the FSM
 */
int8_t FSM_init(button_t *p1 , timer_t *timer , timer_t *pwm , led_t *lg , led_t *lr , led_t *ly);

/**
 * Public function to be called to evolve the FSM Status
 * This function allows to read the inputs and update the current status and the output
 * The function must be called iteratively after the machine has been initialized
 */
int8_t FSM_step();


#endif /* INC_FSM_LOGIC_H_ */
