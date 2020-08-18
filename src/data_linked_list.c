// Copyright (C) 2020 Filippo Savi - All Rights Reserved

// This file is part of uscope_driver.

// uscope_driver is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the
// License.

// uscope_driver is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>.

#include "data_linked_list.h"

/// Initialize the data linked list
void init_data_list(void){
    head = calloc(1, sizeof(data_ll_t));
    tail = calloc(1, sizeof(data_ll_t));

    head->next = tail;
    head->prev = NULL;
    tail->next = NULL;
    tail->prev = head;

}
/// Insert the data pointed by the parameter at the front of the list.
/// \param data Data to be inserted in the list
void data_push_front(int32_t *data){
    data_ll_t *node = calloc(1, sizeof(data_ll_t));
    node->next = head->next;
    head->next = node;
    node->prev = head;
    node->next->prev = node;
    memcpy(node->data, data, 1024* sizeof(int32_t));
    list_length++;
}

/// Insert the data pointed by the parameter at the back of the list.
/// \param data Data to be inserted in the list
void data_push_back(int32_t *data){
    data_ll_t *node = calloc(1, sizeof(data_ll_t));
    node->prev = tail->prev;
    tail->prev = node;
    node->prev->next = node;
    node->next = tail;
    memcpy(node->data, data, 1024* sizeof(int32_t));
    list_length++;
}

/// Remove a data node from the front of the list.
/// \return Pointer to the data contained in the node
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

/// get the list length
/// \return List length
int get_data_list_length(void){
    return list_length;
}

/// Remove a data node from the back of the list.
/// \return Pointer to the data contained in the node
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

/// Free all elements of the data list
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