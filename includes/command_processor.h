//
// Created by fils on 2019/11/13.
//

#ifndef USCOPE_DRIVER_COMMAND_PROCESSOR_H
#define USCOPE_DRIVER_COMMAND_PROCESSOR_H

#include <string.h>

#include "commands.h"
#include "fpga_bridge.h"

response_t *process_command(command_t *command);

uint32_t process_load_bitstream(char *operand);
uint32_t process_single_write_register(char *operand_1, char *operand_2);
uint32_t process_proxied_single_write_register(char *operand_1, char *operand_2);
uint32_t process_single_read_register(char *operand_1, response_t *response);
uint32_t process_bulk_write_register(char *operand_1, char *operand_2);
uint32_t process_bulk_read_register(char *operand_1, response_t *response);
uint32_t process_start_capture(char *operand);

#endif //USCOPE_DRIVER_COMMAND_PROCESSOR_H
