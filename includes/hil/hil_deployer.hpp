

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


struct input_metadata_t{
    std::string core;
    uint64_t const_ip_addr;
    uint32_t dest;
    bool is_float;
};

class hil_deployer {
public:
    hil_deployer(std::shared_ptr<fpga_bridge>  &h);

    void set_cores_rom_location(uint64_t base, uint64_t offset){
        cores_rom_base_address = base;
        cores_rom_offset = offset;
    };
    void set_cores_control_location(uint64_t base, uint64_t offset){
        cores_control_base_address = base;
        cores_control_offset = offset;
    };
    void set_cores_inputs_location(uint64_t base, uint64_t offset){
        cores_inputs_base_address = base;
        cores_inputs_offset = offset;
    };
    void set_scope_mux_base(uint64_t base){
        scope_mux_base = base;
    };
    void set_dma_location(uint64_t base, uint64_t offset){
        dma_base_address = base;
        dma_offset = offset;
    };

    void set_sequencer_location(uint64_t sequencer){
        sequencer_address = sequencer;
    };

    responses::response_code deploy(nlohmann::json &spec);
    void setup_inputs(const std::string &core, nlohmann::json &inputs);
    void select_output(uint32_t channel, uint32_t address);
    void set_input(uint32_t address, uint32_t value, std::string core);
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
    void write_register(uint64_t addr, uint32_t val);

    std::map<std::uint16_t, std::pair<std::string, uint16_t>> bus_address_index;


    std::map<std::string, uint32_t> cores_idx;
    std::vector<input_metadata_t> inputs;
    hil_bus_map bus_map;

    uint64_t cores_rom_base_address;
    uint64_t cores_control_base_address;
    uint64_t cores_inputs_base_address;
    uint64_t dma_base_address;
    uint64_t sequencer_address;
    uint64_t scope_mux_base;

    uint64_t cores_rom_offset;
    uint64_t cores_control_offset;
    uint64_t cores_inputs_offset;
    uint64_t dma_offset;

    std::vector<uint32_t> n_channels;
    bool full_cores_override;

    std::shared_ptr<fpga_bridge> hw;
};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
