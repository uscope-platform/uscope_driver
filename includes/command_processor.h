// Copyright (C) 2020 Filippo Savi - All Rights Reserved

// This file is part of uscope_driver.

// uscope_driver is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation, either version 3 of the
// License.

// uscope_driver is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>.

#ifndef USCOPE_DRIVER_COMMAND_PROCESSOR_H
#define USCOPE_DRIVER_COMMAND_PROCESSOR_H

#include <string.h>

#include "commands.h"
#include "fpga_bridge.h"

void process_command(command_t *command, response_t * response);

uint32_t process_load_bitstream(char *operand);
uint32_t process_single_write_register(char *operand_1, char *operand_2);
uint32_t process_proxied_single_write_register(char *operand_1, char *operand_2);
uint32_t process_single_read_register(char *operand_1, response_t *response);
uint32_t process_bulk_write_register(char *operand_1, char *operand_2);
uint32_t process_bulk_read_register(char *operand_1, response_t *response);
uint32_t process_start_capture(char *operand);
uint32_t process_read_data(response_t *response);
uint32_t process_check_capture_progress(response_t *response);

#endif //USCOPE_DRIVER_COMMAND_PROCESSOR_H
