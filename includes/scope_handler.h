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

#ifndef USCOPE_DRIVER_SCOPE_HANDLER_H
#define USCOPE_DRIVER_SCOPE_HANDLER_H


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "server_connector.h"
#include "data_linked_list.h"

#define MODE_RUN 1
#define MODE_CAPTURE 2

void handle_scope_data(int fd, short what, void *arg);
void init_scope_handler(char * driver_file, int32_t buffer_size);
int wait_for_Interrupt(void);
void handler_read_data(int32_t *data, int size);
void cleanup_scope_handler(void);
void start_capture_mode(int n_buffers);
int check_capture_progress(void);

void * data_writeback(void* args);

atomic_bool scope_data_ready;
atomic_bool writeback_done;

volatile int32_t* buffer;
volatile uint32_t fd_data;
int32_t *scope_data_buffer;

int scope_mode;
int n_buffers_left;

int internal_buffer_size;

pthread_t writeback_thread;

#endif //USCOPE_DRIVER_SCOPE_HANDLER_H

