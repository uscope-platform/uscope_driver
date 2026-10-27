//   Copyright 2024 Filippo Savi <filssavi@gmail.com>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.


#ifndef USCOPE_DRIVER_BUS_ACCESSOR_HPP
#define USCOPE_DRIVER_BUS_ACCESSOR_HPP

#include <cstdint>
#include <csignal>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>
#include <string>
#include <atomic>

#include "errors/result.hpp"

#include "hw_interface/interfaces_dictionary.hpp"

#define ZYNQ_REGISTERS_BASE_ADDR 0x43c00000
#define ZYNQ_FCORE_BASE_ADDR 0x83c00000
#define ZYNQMP_REGISTERS_BASE_ADDR 0x400000000
#define ZYNQMP_FCORE_BASE_ADDR 0x500000000


enum bus_access_type {control_plane_write, rom_plane_write, control_plane_read, rom_plane_read};

struct bus_op{
    std::vector<uint64_t> address;
    std::vector<uint64_t> data;
    bus_access_type type;
};

class bus_accessor {
public:
    /// Maps the buses of the architecture selected with the ARCH environment variable
    static fcore::result<std::shared_ptr<bus_accessor>> create();

    /// Only create() can build the key, which keeps the constructor out of the reach of the callers
    class construction_key {
        friend class bus_accessor;
        construction_key() = default;
    };
    explicit bus_accessor(construction_key) {}

    /// The accesses that fall outside of the mapped windows are rejected with an error, nothing is written
    [[nodiscard]] fcore::result<void> load_program(uint64_t address, const std::vector<uint32_t> program);
    [[nodiscard]] fcore::result<void> write_register(const std::vector<uint64_t>& addresses, uint64_t data);
    [[nodiscard]] fcore::result<uint32_t> read_register(const std::vector<uint64_t>& address);

    [[nodiscard]] fcore::result<uint64_t> register_address_to_index(uint64_t address) const;
    [[nodiscard]] fcore::result<uint64_t> fcore_address_to_index(uint64_t address) const;

    std::vector<bus_op> get_operations() {return operations;}
    std::pair<std::string, std::string> get_hardware_simulation_data();

    void disable_access() {previous_sink_mode = sink_mode; sink_mode = true;}
    void enable_access() {sink_mode = previous_sink_mode;}
    void clear_operations() {operations.clear();}
private:
    fcore::result<void> map_buses();

    bool previous_sink_mode = false;
    uint64_t control_addr = 0, core_addr = 0;
    // size in 32 bit words of the windows mapped over the control and fcore programming buses
    uint64_t control_words = 0;
    uint64_t fcore_words = 0;

    volatile uint32_t *registers = nullptr;
    volatile uint32_t *fCore = nullptr;
    static std::atomic<bool> sink_mode;
    std::vector<bus_op> operations;
};


#endif //USCOPE_DRIVER_BUS_ACCESSOR_HPP
