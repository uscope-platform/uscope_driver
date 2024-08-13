

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

#ifndef USCOPE_DRIVER_DEPLOYER_BASE_HPP
#define USCOPE_DRIVER_DEPLOYER_BASE_HPP

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "hw_interface/fpga_bridge.hpp"
#include "deployment/hil_bus_map.hpp"

class deployer_base {
public:
    deployer_base(std::shared_ptr<fpga_bridge>  &h);
protected:

    void write_register(uint64_t addr, uint32_t val);
    void load_core(uint64_t address, const std::vector<uint32_t> &program);
    void setup_core(uint64_t core_address, uint32_t n_channels);
    void setup_memories(uint64_t address, const std::vector<fcore::emulator::emulator_memory_specs> &init_val);

    uint16_t setup_output_dma(uint64_t address, const std::string& core_name);
    void setup_output_entry(const bus_map_entry &e, uint64_t dma_address, uint32_t io_progressive);

    static uint32_t get_metadata_value(uint8_t size, bool is_signed, bool is_float);

    std::map<std::string, uint32_t> cores_idx;
    std::shared_ptr<fpga_bridge> hw;
    hil_bus_map bus_map;

};


#endif //USCOPE_DRIVER_DEPLOYER_BASE_HPP
