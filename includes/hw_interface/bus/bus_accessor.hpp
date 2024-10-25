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
#include <spdlog/spdlog.h>

#include "hw_interface/interfaces_dictionary.hpp"

#define ZYNQ_REGISTERS_BASE_ADDR 0x43c00000
#define ZYNQ_FCORE_BASE_ADDR 0x83c00000
#define ZYNQMP_REGISTERS_BASE_ADDR 0x400000000
#define ZYNQMP_FCORE_BASE_ADDR 0x500000000

class bus_accessor {
public:
    bus_accessor();
    void load_program(uint64_t address, const std::vector<uint32_t> program);
    void write_register(const std::vector<uint64_t>& addresses, uint64_t data);
    uint32_t read_register(const std::vector<uint64_t>& address);

    uint64_t register_address_to_index(uint64_t address) const;
    uint64_t fcore_address_to_index(uint64_t address) const;

private:

    uint64_t control_addr, core_addr;

    volatile uint32_t *registers;
    volatile uint32_t *fCore;
};


#endif //USCOPE_DRIVER_BUS_ACCESSOR_HPP
