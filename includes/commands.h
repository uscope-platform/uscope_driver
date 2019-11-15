//
// Created by fils on 2019/11/13.
//

#ifndef USCOPE_DRIVER_COMMANDS_H
#define USCOPE_DRIVER_COMMANDS_H

#include <stdint.h>
#include <stdlib.h>

#define C_NULL_COMMAND 0
#define C_LOAD_BITSTREAM 1
#define C_SINGLE_REGISTER_WRITE 2
#define C_BULK_REGISTER_WRITE 3
#define C_SINGLE_REGISTER_READ 4
#define C_BULK_REGISTER_READ 5
#define C_START_CAPTURE 6
#define C_PROXIED_WRITE 7
#define C_READ_DATA 8

#define RESP_OK 1
#define RESP_ERR_BITSTREAM_NOT_FOUND 2
#define RESP_DATA_NOT_READY 3

#define RESP_TYPE_INBAND 1
#define RESP_TYPE_OUTBAND 2

typedef struct {
    uint32_t opcode;
    char *operand_1;
    char *operand_2;
} command_t;


typedef struct {
    uint32_t opcode;
    uint32_t type;
    uint32_t return_code;
    uint32_t *body;
    uint32_t body_size;
} response_t;


#endif //USCOPE_DRIVER_COMMANDS_H
