

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
#include <bitset>

#include "hil/hil_bus_map.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "emulator/emulator_manager.hpp"
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

    void set_controller_location(uint64_t controller, uint32_t offset){
        controller_base = controller;
        controller_offset = offset;
        hil_tb_offset = 0;
    };
    void set_hil_control_location(uint64_t base){
        hil_control_base = base;
    };

    responses::response_code deploy(nlohmann::json &spec);
    void setup_inputs(const std::string &core, nlohmann::json &inputs);
    void select_output(uint32_t channel, const output_specs_t& output);
    void set_input(uint32_t address, uint32_t value, std::string core);
    void start();
    void stop();
private:

    void reserve_inputs(std::vector<fcore::emulator::emulator_interconnect> &ic);
    void reserve_outputs(std::vector<fcore::emulator::emulator_core> &cores, std::vector<fcore::program_bundle> &programs);

    void load_core(uint64_t address, const std::vector<uint32_t> &program);
    uint16_t setup_output_dma(uint64_t address, const std::string& core_name);
    void setup_output_entry(const bus_map_entry &e, uint64_t dma_address, uint32_t io_progressive);
    void setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& orders);
    void setup_cores(uint16_t n_cores);
    void setup_initial_state(uint64_t address, const std::vector<fcore::emulator::emulator_memory_specs> &init_val);

    void check_reciprocal(const std::vector<uint32_t> &program);
    std::vector<uint32_t> calculate_timebase_divider(const std::vector<fcore::program_bundle> &programs,
                                                                   std::vector<uint32_t> n_c);

    std::vector<uint32_t> calculate_timebase_shift(const std::vector<fcore::program_bundle> &programs,
                                                     std::vector<uint32_t> n_c);

    uint64_t get_core_rom_address(uint16_t core_idx) const;
    uint64_t get_core_control_address(uint16_t core_idx) const;
    uint64_t get_dma_address(uint16_t dma_idx) const;
    uint32_t pack_address_mapping(uint16_t, uint16_t) const;
    void write_register(uint64_t addr, uint32_t val);

    float hil_clock_frequency = 100e6;

    std::map<std::string, uint32_t> cores_idx;
    std::vector<input_metadata_t> inputs;
    hil_bus_map bus_map;

    uint64_t cores_rom_base_address;
    uint64_t cores_control_base_address;
    uint64_t cores_inputs_base_address;
    uint64_t dma_base_address;
    uint64_t controller_base;
    uint64_t hil_tb_offset;
    uint64_t controller_offset;

    uint64_t scope_mux_base;
    uint64_t hil_control_base;

    uint64_t cores_rom_offset;
    uint64_t cores_control_offset;
    uint64_t cores_inputs_offset;
    uint64_t dma_offset;

    std::vector<uint32_t> n_channels;

    double timebase_frequency;
    double timebase_divider = 1;

    bool full_cores_override;

    std::shared_ptr<fpga_bridge> hw;
};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
