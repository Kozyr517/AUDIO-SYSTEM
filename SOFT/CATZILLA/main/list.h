/*
 * list.h
 *
 *  Created on: Nov 15, 2025
 *      Author: Jackson
 */

#ifndef LIST_H_
#define LIST_H_

#include "stdint.h"
#include "stdlib.h"


typedef enum { STATUS_OK, STATUS_ERR } StatusType;

typedef struct ListItem {
	uint32_t *p_data;

	struct ListItem *next_item;
} ListItem;

typedef struct ListHead {
	ListItem *first_item;
	ListItem *last_item;
	uint32_t list_size;

} ListHead;

uint8_t list_init(ListHead *p_head);
uint8_t list_apend(ListHead *p_head, uint32_t *p_data);
uint32_t *list_get_item(ListHead *p_head, uint32_t num);
uint8_t list_delete(ListHead *p_head, uint32_t num);
void list_clear(ListHead *p_head);

#endif /* LIST_H_ */
