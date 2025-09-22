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
#include <atomic>

#include <sys/mman.h>
#include <fcntl.h>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "server_frontend/infrastructure/command.hpp"
#include "server_frontend/infrastructure/response.hpp"
#include "hw_interface/interfaces_dictionary.hpp"
#include "configuration.hpp"


#if defined(__aarch64__) || defined(_M_ARM64)
    constexpr bool on_target = true;
#elif defined(__x86_64__) || defined(_M_X64)
    constexpr bool on_target = false;
#endif

#include "bus/bus_accessor.hpp"

void sigsegv_handler(int dummy);
void sigbus_handler(int dummy);

class fpga_bridge {
public:
    fpga_bridge();
    void set_accessor(const std::shared_ptr<bus_accessor>& bus_acc) {busses = bus_acc;}

    void clear_operations_vector(){busses->clear_operations();}
    responses::response_code load_bitstream(const std::string& bitstream);
    responses::response_code single_write_register(const nlohmann::json &write_obj);
    nlohmann::json single_read_register(uint64_t address);
    responses::response_code apply_program(uint64_t address, std::vector<uint32_t> program);
    responses::response_code apply_filter(uint64_t address, std::vector<uint32_t> taps);
    responses::response_code set_scope_data(uint64_t address);
    std::string get_module_version();
    std::string get_hardware_version();

    void write_direct(uint64_t addr, uint32_t val);
    void write_proxied(uint64_t proxy_addr, uint32_t target_addr, uint32_t val);
    uint32_t read_direct(uint64_t address);

    uint32_t get_pl_clock( uint8_t clk_n);
    responses::response_code  set_pl_clock(uint8_t clk_n, uint32_t freq);

    std::vector<bus_op> get_bus_operations() const;

    std::pair<std::string, std::string> get_hardware_simulation_data() const;


    void disable_bus_access() const {busses->disable_access(); busses->clear_operations();}
    void enable_bus_access() const {busses->enable_access();}
private:

    std::shared_ptr<bus_accessor> busses;
    static std::atomic<bool> fpga_loaded;

    std::string arch;

};


#endif //USCOPE_DRIVER_FPGA_BRIDGE_HPP

