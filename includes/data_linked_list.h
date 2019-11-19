//
// Created by fils on 2019/11/18.
//

#ifndef USCOPE_DRIVER_DATA_LINKED_LIST_H
#define USCOPE_DRIVER_DATA_LINKED_LIST_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct data_ll data_ll_t;

struct data_ll {
    data_ll_t  *prev;
    int32_t data[1024];
    data_ll_t *next;

};


data_ll_t *head;
data_ll_t *tail;

int list_length;

void init_data_list(void);
void data_push_front(int32_t *data);
void data_push_back(int32_t *data);
data_ll_t *data_pop_front(void);
data_ll_t *data_pop_back(void);
void delete_data_list(void);
int get_data_list_length(void);

#endif //USCOPE_DRIVER_DATA_LINKED_LIST_H
