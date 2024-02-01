

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

#ifndef USCOPE_DRIVER_HIL_DEPLOYER_HPP
#define USCOPE_DRIVER_HIL_DEPLOYER_HPP

#include <nlohmann/json.hpp>
#include "hil/hil_bus_map.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "frontend/emulator_manager.hpp"


class hil_deployer {
public:
    hil_deployer(std::shared_ptr<fpga_bridge>  &h);
    void deploy(nlohmann::json &spec);
    void set_cores_location(uint64_t base, uint64_t offset);
    void set_dma_location(uint64_t base, uint64_t offset);
private:
    std::map<std::uint16_t, std::pair<std::string, uint16_t>> bus_address_index;
    uint16_t get_free_address(uint16_t original_addr, const std::string &c_n);
    void load_core(uint64_t address, const std::vector<uint32_t> &program);
    void setup_output_dma(uint64_t address, hil_bus_map &bm, std::set<io_map_entry> cm, std::string core_name);
    void setup_output_entry(uint16_t io_addr, uint16_t bus_address, uint64_t dma_address, uint32_t io_progressive);

    uint64_t get_core_address(uint16_t core_idx) const;
    uint64_t get_dma_address(uint16_t dma_idx) const;
    uint32_t pack_address_mapping(uint16_t, uint16_t) const;
    std::string to_hex(uint64_t)const;
    void write_register(uint64_t addr, uint32_t val);

    uint64_t cores_base_address;
    uint64_t dma_base_address;

    uint64_t cores_offset;
    uint64_t dma_offset;
    std::shared_ptr<fpga_bridge> hw;
};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
