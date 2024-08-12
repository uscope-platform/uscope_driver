

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


struct logic_layout{

    struct logic_layout_bases{

        uint64_t cores_rom;
        uint64_t cores_control;
        uint64_t cores_inputs;
        uint64_t dma;
        uint64_t controller;

        uint64_t scope_mux;
        uint64_t hil_control;

    };
    struct logic_layout_offsets{

        uint64_t hil_tb;
        uint64_t controller;
        uint64_t cores_rom;
        uint64_t cores_control;
        uint64_t cores_inputs;
        uint64_t dma;

    };

    void parse_layout_object(const nlohmann::json &obj){
        bases.cores_rom = obj["bases"]["cores_rom"];
        bases.cores_control = obj["bases"]["cores_control"];
        bases.cores_inputs = obj["bases"]["cores_inputs"];
        bases.dma = obj["bases"]["dma"];
        bases.controller = obj["bases"]["controller"];
        bases.scope_mux = obj["bases"]["scope_mux"];
        bases.hil_control = obj["bases"]["hil_control"];


        offsets.cores_rom = obj["offsets"]["cores_rom"];
        offsets.cores_control = obj["offsets"]["cores_control"];
        offsets.controller = obj["offsets"]["controller"];
        offsets.cores_inputs = obj["offsets"]["cores_inputs"];
        offsets.dma = obj["offsets"]["dma"];
        offsets.hil_tb = obj["offsets"]["hil_tb"];
    };


    logic_layout_bases bases;
    logic_layout_offsets offsets;
};


class hil_deployer {
public:
    hil_deployer(std::shared_ptr<fpga_bridge>  &h);

    void set_layout_map(nlohmann::json &obj);

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

    static uint32_t get_metadata_value(uint8_t size, bool is_signed, bool is_float);

    void write_register(uint64_t addr, uint32_t val);

    float hil_clock_frequency = 100e6;

    std::map<std::string, uint32_t> cores_idx;
    std::vector<input_metadata_t> inputs;
    hil_bus_map bus_map;


    logic_layout addresses;

    std::vector<uint32_t> n_channels;

    double timebase_frequency;
    double timebase_divider = 1;

    bool full_cores_override;

    std::shared_ptr<fpga_bridge> hw;
};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
