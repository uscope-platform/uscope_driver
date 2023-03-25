// Copyright 2021 University of Nottingham Ningbo China
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef USCOPE_DRIVER_FPGA_BRIDGE_HPP
#define USCOPE_DRIVER_FPGA_BRIDGE_HPP

#include <cstring>
#include <csignal>

#include <string>
#include <iostream>
#include <filesystem>

#include <sys/mman.h>
#include <fcntl.h>

#include <nlohmann/json.hpp>

#include "command.hpp"
#include "response.hpp"
#include "scope_thread.hpp"

#define REGISTERS_BASE_ADDR 0x43c00000
#define FCORE_BASE_ADDR 0x83c00000

void sigsegv_handler(int dummy);
void sigbus_handler(int dummy);

class fpga_bridge {
public:
    fpga_bridge(const std::string& driver_file, unsigned int dma_buffer_size, bool emulate_control, bool log);
    responses::response_code load_bitstream(const std::string& bitstream);
    responses::response_code single_write_register(const nlohmann::json &write_obj);
    nlohmann::json single_read_register(uint32_t address);
    responses::response_code start_capture(uint32_t n_buffers);
    responses::response_code read_data(std::vector<float> &read_data);
    responses::response_code apply_program(uint32_t address, std::vector<uint32_t> program);
    responses::response_code set_channel_widths( std::vector<uint32_t> widths);
    responses::response_code set_scaling_factors( std::vector<float> sfs);
    responses::response_code set_clock_frequency( std::vector<uint32_t> freq);

    int check_capture_progress(unsigned int &progress);
    void stop_scope();
    static uint32_t register_address_to_index(uint32_t address);
    static uint32_t fcore_address_to_index(uint32_t address);

private:
    int registers_fd, fcore_fd;
    
    volatile uint32_t *registers;
    volatile uint32_t *fCore;

    bool debug_mode;
    bool log_enabled;


    scope_thread scope_handler;
};


#endif //USCOPE_DRIVER_FPGA_BRIDGE_HPP

