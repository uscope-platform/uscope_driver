

#include "data_linked_list.h"

void init_data_list(void){
    head = calloc(1, sizeof(data_ll_t));
    tail = calloc(1, sizeof(data_ll_t));

    head->next = tail;
    head->prev = NULL;
    tail->next = NULL;
    tail->prev = head;

}

void data_push_front(int32_t *data){
    data_ll_t *node = calloc(1, sizeof(data_ll_t));
    node->next = head->next;
    head->next = node;
    node->prev = head;
    node->next->prev = node;
    memcpy(node->data, data, 1024* sizeof(int32_t));
    list_length++;
}

void data_push_back(int32_t *data){
    data_ll_t *node = calloc(1, sizeof(data_ll_t));
    node->prev = tail->prev;
    tail->prev = node;
    node->prev->next = node;
    node->next = tail;
    memcpy(node->data, data, 1024* sizeof(int32_t));
    list_length++;
}

data_ll_t *data_pop_front(void){
    data_ll_t *ret = head->next;
    if(ret->next != tail){
        head->next = ret->next;
        ret->next->prev = head;
    }
    ret->prev = NULL;
    ret->next = NULL;
    list_length--;
    return ret;
}

int get_data_list_length(void){
    return list_length;
}

data_ll_t *data_pop_back(void){
    data_ll_t *ret = tail->prev;
    if(tail->prev != head){
        tail->prev = ret->prev;
        ret->prev->next = tail;
    }
    ret->prev = NULL;
    ret->next = NULL;
    list_length--;
    return ret;
}

void delete_data_list(void){
    tail = NULL;
    data_ll_t *current_node = data_pop_back();
    if(current_node->prev==head){
        free(current_node);
        head = NULL;
        return;
    } else{
        free(current_node);
        delete_data_list();
    }
}