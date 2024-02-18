

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

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
#include "hil/hil_bus_map.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "frontend/emulator_manager.hpp"
#include "fCore_isa.hpp"


class hil_deployer {
public:
    hil_deployer(std::shared_ptr<fpga_bridge>  &h);
    responses::response_code deploy(nlohmann::json &spec);
    void set_cores_rom_location(uint64_t base, uint64_t offset);
    void set_cores_control_location(uint64_t base, uint64_t offset);
    void set_dma_location(uint64_t base, uint64_t offset);
    void set_sequencer_location(uint64_t sequencer);

private:
    uint16_t get_free_address(uint16_t original_addr, const std::string &c_n);

    void reserve_inputs(std::vector<fcore::interconnect_t> &ic);
    void reserve_outputs(std::vector<fcore::program_bundle> &programs);

    void load_core(uint64_t address, const std::vector<uint32_t> &program);
    uint16_t setup_output_dma(uint64_t address, const std::string& core_name);
    void setup_output_entry(uint16_t io_addr, uint16_t bus_address, uint64_t dma_address, uint32_t io_progressive);
    void setup_sequencer(uint64_t seq, uint16_t n_cores, uint16_t n_transfers);
    void setup_cores(uint16_t n_cores);
    void setup_initial_state(uint64_t address, const std::unordered_map<uint32_t, uint32_t> &init_val);

    void check_reciprocal(const std::vector<uint32_t> &program);

    uint64_t get_core_rom_address(uint16_t core_idx) const;
    uint64_t get_core_control_address(uint16_t core_idx) const;
    uint64_t get_dma_address(uint16_t dma_idx) const;
    uint32_t pack_address_mapping(uint16_t, uint16_t) const;
    std::string to_hex(uint64_t)const;
    void write_register(uint64_t addr, uint32_t val);

    std::map<std::uint16_t, std::pair<std::string, uint16_t>> bus_address_index;

    hil_bus_map bus_map;

    uint64_t cores_rom_base_address;
    uint64_t cores_control_base_address;
    uint64_t dma_base_address;
    uint64_t sequencer_address;

    uint64_t cores_rom_offset;
    uint64_t cores_control_offset;
    uint64_t dma_offset;

    std::vector<uint32_t> n_channels;
    bool full_cores_override;

    std::shared_ptr<fpga_bridge> hw;
};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
