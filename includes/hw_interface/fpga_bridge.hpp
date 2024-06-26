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
#include <utility>
#include <string>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>

#include <sys/mman.h>
#include <fcntl.h>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "server_frontend/infrastructure/command.hpp"
#include "server_frontend/infrastructure/response.hpp"
#include "hw_interface/interfaces_dictionary.hpp"
#include "configuration.hpp"

#define ZYNQ_REGISTERS_BASE_ADDR 0x43c00000
#define ZYNQ_FCORE_BASE_ADDR 0x83c00000
#define ZYNQMP_REGISTERS_BASE_ADDR 0x400000000
#define ZYNQMP_FCORE_BASE_ADDR 0x500000000

void sigsegv_handler(int dummy);
void sigbus_handler(int dummy);

class fpga_bridge {
public:
    fpga_bridge();
    responses::response_code load_bitstream(const std::string& bitstream);
    responses::response_code single_write_register(const nlohmann::json &write_obj);
    nlohmann::json single_read_register(uint64_t address);
    responses::response_code apply_program(uint64_t address, std::vector<uint32_t> program);
    responses::response_code apply_filter(uint64_t address, std::vector<uint32_t> taps);
    responses::response_code set_clock_frequency(std::vector<uint32_t> freq);
    responses::response_code set_scope_data(uint64_t address);
    std::string get_module_version();
    std::string get_hardware_version();

    void write_direct(uint64_t addr, uint32_t val);
    void write_proxied(uint64_t proxy_addr, uint32_t target_addr, uint32_t val);
    uint32_t read_direct(uint64_t address);

    uint64_t register_address_to_index(uint64_t address) const;
    uint64_t fcore_address_to_index(uint64_t address) const;

    uint32_t get_pl_clock( uint8_t clk_n);
    void set_pl_clock(uint8_t clk_n, uint32_t freq);

private:
    volatile uint32_t *registers;
    volatile uint32_t *fCore;
    uint64_t control_addr;
    uint64_t core_addr;


    std::string arch;

};


#endif //USCOPE_DRIVER_FPGA_BRIDGE_HPP

