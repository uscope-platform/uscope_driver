//
// Created by fils on 2019/11/12.
//

#ifndef USCOPE_DIR_USCOPE_DRIVER_H
#define USCOPE_DIR_USCOPE_DRIVER_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <regex.h>
#include "event2/event.h"
#include "hiredis/hiredis.h"
#include "hiredis/async.h"
#include "hiredis/adapters/libevent.h"

#include "commands.h"
#include "command_processor.h"
#include "fpga_bridge.h"
int setup_main_loop(void);



struct event_base *main_loop;



redisContext *reply_channel;


command_t *parse_command(char *received_string);
void free_command(command_t * command);
char *serialize_response(response_t *response);


#endif //USCOPE_DIR_USCOPE_DRIVER_H
