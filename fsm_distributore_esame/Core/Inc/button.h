#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#define BUTTON_INIT_STATE_OFF		(0)
#define BUTTON_INIT_STATE_ON		(1)
#define BUTTON_INIT_DELAY			(0)

#define BUTTON_ERR		(-1)
#define BUTTON_OK		(0)

typedef GPIO_PinState button_state_t;

struct button_s
{
	GPIO_TypeDef* GPIOx;
	uint16_t GPIO_Pin;
	button_state_t state;
	button_state_t last_state;
	uint32_t delay;
	uint32_t last_tick;
};

typedef struct button_s button_t;

/*
 * Init does not allocate the button structure
 * Always allocate resources outside the functions
 */
int8_t button_init(button_t* button, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, button_state_t init_state);

/*
 * Read the value
 */
int8_t button_read(button_t* button, button_state_t* state);

/*
 * Set the delay in milliseconds while reading the button
 */
int8_t button_set_delay(button_t* button, uint32_t read_delay);

/*
 * Get the delay in milliseconds while reading the button
 */
int8_t button_get_delay(button_t* button, uint32_t* read_delay);

/*
 * Return the port number of GPIO PORT used by the button
 */
int8_t button_get_port(const button_t* button, GPIO_TypeDef** port);

/*
 * Return the port number of GPIO PIN NUMBER used by the button
 */
int8_t button_get_pin_number(const button_t* button, uint16_t* pin);

#endif /* INC_BUTTON_H_ */
