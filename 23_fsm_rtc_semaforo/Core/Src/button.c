#include <stdint.h>
#include "gpio.h"
#include "button.h"

int8_t button_init(button_t* button, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, button_state_t init_state)
{

	int8_t res = BUTTON_ERR;

	if(button)
	{
		button->GPIO_Pin = GPIO_Pin;		// Pin number
		button->GPIOx = GPIOx;				// Port number
		button->delay = BUTTON_INIT_DELAY;	// Minimum time interval with no state variations
		button->last_tick = 0;				//
		button->state = init_state;
		button->last_state = init_state;
		res = BUTTON_OK;
	}

	return res;
}

int8_t button_set_delay(button_t* button, uint32_t read_delay)
{
	int8_t res = BUTTON_ERR;

	if(button)
	{
		button->delay = read_delay;
		res = BUTTON_OK;
	}

	return res;
}

int8_t button_get_delay(button_t* button, uint32_t* read_delay)
{
	int8_t res = BUTTON_ERR;

	if(button && read_delay)
	{
		*read_delay = button->delay;
		res = BUTTON_OK;
	}

	return res;
}

int8_t button_read(button_t* button, button_state_t* state)
{

	int8_t res = BUTTON_ERR;
	button_state_t pin_state = 0;
	uint32_t elapsed_tick = -1;
	uint32_t current_tick = -1;

	if(button && state)
	{
		current_tick = HAL_GetTick();
		elapsed_tick = current_tick - button->last_tick;
		pin_state = HAL_GPIO_ReadPin(button->GPIOx, button->GPIO_Pin);

		if(pin_state != button->last_state || pin_state == button->state)
		{
			button->last_tick = current_tick;
		}

		if(elapsed_tick > button->delay)
		{
			if(pin_state != button->state)
			{
				button->state = pin_state;
			}
		}

		button->last_state = pin_state;
		*state = button->state;
		res = BUTTON_OK;
	}

	return res;
}

inline int8_t button_get_port(const button_t* button, GPIO_TypeDef** port)
{
	int8_t res = BUTTON_ERR;

	if(button)
	{
		*port = button->GPIOx;
		res = BUTTON_OK;
	}

	return res;
}

inline int8_t button_get_pin_number(const button_t* button, uint16_t* pin)
{
	int8_t res = BUTTON_ERR;

	if(button)
	{
		*pin = button->GPIO_Pin;
		res = BUTTON_OK;
	}

	return res;
}


int8_t button_read_pressed(button_t* button, button_state_t* state)
{
	int8_t res = BUTTON_ERR;

	if(button)
	{
		*state = button->state;
		res = BUTTON_OK;
	}

	return res;
}


int8_t button_reset_pressed(button_t* button)
{
	int8_t res = BUTTON_ERR;

	if(button)
	{
		button->state = RELEASED;
		res = BUTTON_OK;
	}

	return res;
}



int8_t button_pressed_handler(button_t* button, uint16_t pin)
{
	int8_t res = BUTTON_ERR;
	if(button)
	{
		if(pin == button->GPIO_Pin)
		{
			button->state = PRESSED;
		}
		res = BUTTON_OK;
	}
	return res;
}

int8_t button_get_and_clear_pressed(button_t* button, button_state_t* state)
{
    int8_t res = BUTTON_ERR;

    if(button && state)
    {
        __disable_irq(); // Inizio sezione critica

        *state = button->state;
        button->state = RELEASED; // Reset atomico

        __enable_irq(); // Fine sezione critica
        res = BUTTON_OK;
    }

    return res;
}
