/*
 * queue.h
 *
 *  Created on: Apr 22, 2025
 *      Author: vcarletti
 */

#ifndef INC_QUEUE_H_
#define INC_QUEUE_H_

#include "stdlib.h"
#include "stdint.h"

#define QUEUE_OK	(0)
#define QUEUE_ERR	(1)
#define QUEUE_EMPTY	(2)
#define QUEUE_FULL	(2)

struct queue_s
{
	uint16_t qsize;	  //Queue in terms of max number of items
					  //Real size is isize*qsize
	uint16_t length;
	uint16_t head;
	uint16_t tail;
	uint16_t isize;  //Item size in byte
	uint8_t* items;
};

typedef struct queue_s queue_t;

uint8_t queue_init(queue_t* queue, uint8_t* queue_buffer, uint16_t items_size, uint16_t queue_size);
//Read the head element
uint8_t queue_head(queue_t*, void* item);

uint8_t queue_enqueue(queue_t* queue, const void* item);

//Extract and remove the head
uint8_t queue_extract(queue_t* queue, void* item);

//Move the head pointer
uint8_t queue_dequeue(queue_t* queue);

uint8_t queue_get_length(const queue_t* queue, uint16_t* length);
uint8_t queue_get_size(const queue_t* queue, uint16_t* size);
uint8_t queue_get_item_size(const queue_t* queue, uint16_t* size);
uint8_t queue_is_empty(const queue_t* queue);
uint8_t queue_is_full(const queue_t* queue);



#endif /* INC_QUEUE_H_ */
