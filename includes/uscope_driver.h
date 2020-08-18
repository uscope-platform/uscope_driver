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

#ifndef USCOPE_DIR_USCOPE_DRIVER_H
#define USCOPE_DIR_USCOPE_DRIVER_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <regex.h>
#include <argp.h>
#include "event2/event.h"
#include "hiredis/hiredis.h"
#include "hiredis/async.h"
#include "hiredis/adapters/libevent.h"

#include <unistd.h>
#include <sys/mman.h>


#include "commands.h"
#include "command_processor.h"
#include "fpga_bridge.h"
#include "scope_handler.h"

int setup_main_loop(void);



struct event_base *main_loop;



redisContext *reply_channel;

bool debug_mode;

int sh_mem_fd;
volatile uint32_t *shared_memory;
struct event *signal_int;
struct event *scope_data;

command_t *parse_command(char *received_string);
void respond(response_t *response);


#endif //USCOPE_DIR_USCOPE_DRIVER_H
