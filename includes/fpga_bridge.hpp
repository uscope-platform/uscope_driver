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
#ifndef USCOPE_DRIVER_FPGA_BRIDGE_HPP
#define USCOPE_DRIVER_FPGA_BRIDGE_HPP

#include <cstring>

#include <string>
#include <iostream>
#include <filesystem>

#include <sys/mman.h>
#include <fcntl.h>

#include "commands.hpp"

#include "scope_thread.hpp"

#define BASE_ADDR 0x43c00000

class fpga_bridge {

public:
    fpga_bridge(const std::string& driver_file, unsigned int dma_buffer_size, bool debug);
    int load_bitstream(char *bitstream);
    int single_write_register(uint32_t address, uint32_t value);
    int single_read_register(uint32_t address, std::vector<uint32_t> value);
    int bulk_write_register(uint32_t *address, volatile uint32_t *value, volatile uint32_t n_registers);
    int bulk_read_register(uint32_t *address, std::vector<uint32_t> value, volatile uint32_t n_registers);
    int start_capture(uint32_t n_buffers);
    int single_proxied_write_register(uint32_t proxy_address,uint32_t reg_address, uint32_t value);
    int read_data(std::vector<uint32_t> &read_data);
    unsigned int check_capture_progress();
    void stop_scope();
    static uint32_t address_to_index(uint32_t address);

private:
    int regs_fd;
    bool debug_mode;
    volatile uint32_t *registers;
    scope_thread scope_handler;
};


#endif //USCOPE_DRIVER_FPGA_BRIDGE_HPP
