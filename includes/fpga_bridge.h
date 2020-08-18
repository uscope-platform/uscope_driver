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

#ifndef USCOPE_DRIVER_FPGA_BRIDGE_H
#define USCOPE_DRIVER_FPGA_BRIDGE_H

#define DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "commands.h"
#include "scope_handler.h"

#define BASE_ADDR 0x43c00000

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

void init_fpga_bridge(void);
int load_bitstream(char *bitstream);
int single_write_register(uint32_t address, uint32_t value);
int single_read_register(uint32_t address, volatile uint32_t *value);
int bulk_write_register(uint32_t *address, volatile uint32_t *value, volatile uint32_t n_registers);
int bulk_read_register(uint32_t *address, volatile uint32_t *value, volatile uint32_t n_registers);
int start_capture(uint32_t n_buffers);
int single_proxied_write_register(uint32_t proxy_address,uint32_t reg_address, uint32_t value);
int read_data(int32_t * read_data);

uint32_t address_to_index(uint32_t address);

int regs_fd;
volatile uint32_t *registers;



#endif //USCOPE_DRIVER_FPGA_BRIDGE_H
