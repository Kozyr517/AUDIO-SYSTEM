/*
 * list.c
 *
 *  Created on: Nov 15, 2025
 *      Author: Jackson
 */


#include "list.h"


uint8_t list_init(ListHead *p_head) {

	if(p_head == NULL){
		return STATUS_ERR;
	} else {
		p_head->first_item = NULL;
		p_head->last_item = NULL;
		p_head->list_size = 0;
	}

	return STATUS_OK;
}

uint8_t list_apend(ListHead *p_head, uint32_t *p_data) {

	if(p_head == NULL || p_data == NULL) {
		return STATUS_ERR;
	}

	ListItem *ptr;
	ptr = (ListItem*)malloc(sizeof(ListItem));

	if(ptr == NULL) {
		return STATUS_ERR;
	}

	ptr->next_item = NULL;
	ptr->p_data = p_data;

	if(p_head->list_size == 0) {
		p_head->first_item = ptr;
		p_head->last_item = ptr;
		p_head->list_size += 1;

	} else {
		p_head->last_item->next_item = ptr;
		p_head->last_item = ptr;
		p_head->list_size += 1;
	}

	return STATUS_OK;
}

uint32_t* list_get_item(ListHead *p_head, uint32_t num) {

	if(p_head == NULL || num == 0) {
		return NULL;
	}
	if(num > p_head->list_size) {
		return NULL;
	}

	if(num == 1) {
		return p_head->first_item->p_data;
	}

	ListItem *curent_ptr = p_head->first_item;

	for(uint32_t i=1; i<num; i++) {
		curent_ptr = curent_ptr->next_item;
	}

	return curent_ptr->p_data;
}

uint8_t list_delete(ListHead *p_head, uint32_t num) {

    if (p_head == NULL || num == 0) {
        return STATUS_ERR;
    }
    if (num > p_head->list_size) {
        return STATUS_ERR;
    }

    ListItem *cur_item = p_head->first_item;
    ListItem *prev_item = NULL;

    if (num == 1) {
        p_head->first_item = cur_item->next_item;
        free(cur_item->p_data);
        free(cur_item);

        if (--p_head->list_size == 0) {
            p_head->last_item = NULL;
        }
        return STATUS_OK;
    }

    for (uint32_t i = 1; i < num; i++) {
        prev_item = cur_item;
        cur_item = cur_item->next_item;
    }

    if (cur_item == p_head->last_item) {
        p_head->last_item = prev_item;
        prev_item->next_item = NULL;
    } else {
        prev_item->next_item = cur_item->next_item;
    }

    free(cur_item->p_data);
    free(cur_item);
    p_head->list_size -= 1;

    return STATUS_OK;
}

void list_clear(ListHead *p_head) {

	int status = STATUS_OK;

	while(status == STATUS_OK) {
		status = list_delete(p_head, 1);
	}
}
