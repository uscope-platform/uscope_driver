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

#ifndef USCOPE_DRIVER_DATA_LINKED_LIST_H
#define USCOPE_DRIVER_DATA_LINKED_LIST_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
    extern "C" {
#endif
        typedef struct data_ll data_ll_t;

        struct data_ll {
            data_ll_t *prev;
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

#ifdef __cplusplus
    }
#endif

#endif //USCOPE_DRIVER_DATA_LINKED_LIST_H
