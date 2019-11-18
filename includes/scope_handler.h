//
// Created by fils on 2019/11/14.
//

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


void handle_scope_data(int fd, short what, void *arg);
void init_scope_handler(char * driver_file, int32_t buffer_size);
int wait_for_Interrupt(void);
void handler_read_data(int32_t *data, int size);
void cleanup_scope_handler(void);

atomic_bool thread_die;
atomic_bool scope_data_ready;

volatile int32_t* buffer;
volatile uint32_t fd_data;
int32_t *scope_data_buffer;

int internal_buffer_size;

#endif //USCOPE_DRIVER_SCOPE_HANDLER_H

