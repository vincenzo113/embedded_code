#include <stdint.h>
#include "tim.h"
#include "timer.h"

int8_t timer_init(timer_t* timer, TIM_HandleTypeDef* handler, uint32_t frequency)
{
	int8_t res = TIMER_ERR;
	if(timer && handler)
	{
		timer->htim = handler;
		timer->running = 0;
		timer->elapsed = 0;
		timer->in_frequency = frequency;
		res = TIMER_OK;
	}
	return res;

}

int8_t timer_start(timer_t* timer)
{
	int8_t res = TIMER_ERR;
	if(timer)
	{
		timer->running = 1;
		timer->elapsed = 0;
		HAL_TIM_Base_Start_IT(timer->htim);
		res = TIMER_OK;
	}
	return res;
}

int8_t timer_stop(timer_t* timer)
{
	int8_t res = TIMER_ERR;
	if(timer)
	{
		timer->running = 0;
		timer->elapsed = 0;
		HAL_TIM_Base_Stop_IT(timer->htim);
		res = TIMER_OK;
	}
	return res;
}

int8_t timer_reset(timer_t* timer)
{
	int8_t res = TIMER_ERR;
	if(timer)
	{
		timer->running = 0;
		timer->elapsed = 0;
		HAL_TIM_Base_Stop_IT(timer->htim);
		__HAL_TIM_SET_COUNTER(timer->htim, 0);
		res = TIMER_OK;
	}
	return res;
}

int8_t timer_is_running(timer_t* timer)
{
	int8_t res = 0;
	if(timer)
	{
		res = timer->running;
	}
	return res;
}

int8_t timer_is_elapsed(timer_t* timer)
{
	int8_t res = 0;

	if(timer)
	{
		res = timer->elapsed;
	}
	return res;
}

int8_t timer_is_my_handler(timer_t* timer, TIM_HandleTypeDef* handler)
{
	int8_t res = TIMER_ERR;
	if(timer && handler)
	{
		TIM_HandleTypeDef* inhandler = (TIM_HandleTypeDef*)handler;
		if(inhandler->Instance == timer->htim->Instance)
		{
			res = TIMER_OK;
		}
	}
	return res;
}

int8_t timer_set_period(timer_t* timer, uint16_t period)
{
	int8_t res = TIMER_ERR;
	uint32_t arr = -1;
	uint32_t prescaler = -1;

	if(timer)
	{
		prescaler = timer->htim->Instance->PSC;
		arr = ((period * timer->in_frequency)/(prescaler+1))-1;
		__HAL_TIM_SET_AUTORELOAD(timer->htim, arr);
		res = TIMER_OK;
	}

	return res;
}

int8_t timer_elapsed_handler(timer_t* timer, TIM_HandleTypeDef* handler)
{
	int8_t res = TIMER_ERR;

	if(timer_is_my_handler(timer, handler) == TIMER_OK)
	{
		if (!timer->timer_elapsed_first) {
			timer->timer_elapsed_first = 1;
		} else {
			timer->running = 0;
			timer->elapsed = 1;
		}
		res = TIMER_OK;
	}

	return res;
}


