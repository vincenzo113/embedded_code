/*
 * queue.c
 *
 *  Created on: Apr 22, 2025
 *      Author: vcarletti
 */
#include "queue.h"
#include "cmsis_gcc.h"
#include "string.h"

static inline void increase_head(queue_t* queue)
{
	queue->head = (queue->head+1)%queue->qsize;
	queue->length--;
}

static inline void extract_data(queue_t* queue, void* item)
{
	size_t offset = queue->head * queue->isize;
	memcpy(item, queue->items+offset, queue->isize);
}

uint8_t queue_init(queue_t* queue, uint8_t* queue_buffer, uint16_t items_size, uint16_t queue_size)
{
	if(!queue || !queue_buffer || items_size == 0 || queue_size == 0)
	{
		return QUEUE_ERR;
	}

	queue->qsize = queue_size;
	queue->isize = items_size;
	queue->length = 0;
	queue->head = 0;
	queue->tail = 0;
	queue->items = queue_buffer;

	return QUEUE_OK;
}

uint8_t queue_enqueue(queue_t* queue, const void* item)
{
	if(!queue || !item)
	{
		return QUEUE_ERR;
	}

	//Entering critical section
	__disable_irq();
	//Not checking if full

	//Insert in the current free slot
	size_t offset = queue->tail * queue->isize;
	memcpy(queue->items+offset, item, queue->isize);

	//Evaluate next free slot
	queue->tail = (queue->tail+1)%queue->qsize;

	if(queue->length < queue->qsize - 1)
	{
		queue->length++;
	}

	//Exiting critical section
	__enable_irq();

	return QUEUE_OK;
}

uint8_t queue_head(queue_t* queue, void* item)
{
	uint8_t ret = QUEUE_ERR;

	if(!queue || !item)
	{
		return QUEUE_ERR;
	}

	__disable_irq();

	if(queue->length > 0)
	{
		//Getting head item by copy
		extract_data(queue, item);

		ret = QUEUE_OK;
	}
	else
	{
		ret = QUEUE_EMPTY;
	}
	__enable_irq();

	return ret;

}

uint8_t queue_extract(queue_t* queue, void* item){

	uint8_t ret = QUEUE_ERR;

	if(!queue || !item)
	{
		return QUEUE_ERR;
	}

	__disable_irq();

	if(queue->length > 0)
	{
		//Getting head item by copy
		extract_data(queue, item);

		//Moving to next item
		increase_head(queue);
		ret = QUEUE_OK;
	}
	else
	{
		ret = QUEUE_EMPTY;
	}
	__enable_irq();

	return ret;

}

uint8_t queue_dequeue(queue_t* queue){

	uint8_t ret = QUEUE_ERR;

	if(!queue)
	{
		return QUEUE_ERR;
	}

	__disable_irq();

	if(queue->length > 0)
	{
		//Moving to next item
		increase_head(queue);
		ret = QUEUE_OK;
	}
	else
	{
		ret = QUEUE_EMPTY;
	}
	__enable_irq();

	return ret;

}

uint8_t queue_get_length(const queue_t* queue, uint16_t* length)
{
	if(!queue || !length)
	{
		return QUEUE_ERR;
	}

	__disable_irq();
	*length = queue->length;
	__enable_irq();
	return QUEUE_OK;
}

uint8_t queue_get_size(const queue_t* queue, uint16_t* size)
{
	if(!queue || !size)
	{
		return QUEUE_ERR;
	}

	__disable_irq();
	*size = queue->qsize;
	__enable_irq();
	return QUEUE_OK;
}

uint8_t queue_get_item_size(const queue_t* queue, uint16_t* size)
{
	if(!queue || !size)
	{
		return QUEUE_ERR;
	}

	__disable_irq();
	*size = queue->isize;
	__enable_irq();
	return QUEUE_OK;
}

uint8_t queue_is_full(const queue_t* queue)
{
	uint8_t ret = QUEUE_ERR;
	if(!queue)
	{
		return QUEUE_ERR;
	}

	__disable_irq();
	if(queue->length >= queue->qsize)
	{
		ret = QUEUE_FULL;
	}
	else
	{
		ret = QUEUE_OK;
	}
	__enable_irq();

	return ret;
}

uint8_t queue_is_empty(const queue_t* queue)
{
	uint8_t ret = QUEUE_ERR;
	if(!queue)
	{
		return QUEUE_ERR;
	}

	__disable_irq();
	if(queue->length == 0)
	{
		ret = QUEUE_EMPTY;
	}
	else
	{
		ret = QUEUE_OK;
	}
	__enable_irq();

	return ret;
}
