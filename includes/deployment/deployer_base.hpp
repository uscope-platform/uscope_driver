

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


struct logic_layout{

    struct logic_layout_bases{

        uint64_t cores_rom;
        uint64_t cores_control;
        uint64_t cores_inputs;
        uint64_t controller;

        uint64_t scope_mux;
        uint64_t hil_control;

    };
    struct logic_layout_offsets{

        uint64_t dma;
        uint64_t hil_tb;
        uint64_t controller;
        uint64_t cores_rom;
        uint64_t cores_control;
        uint64_t cores_inputs;

    };

    void parse_layout_object(const nlohmann::json &obj){
        bases.cores_rom = obj["bases"]["cores_rom"];
        bases.cores_control = obj["bases"]["cores_control"];
        bases.cores_inputs = obj["bases"]["cores_inputs"];
        bases.controller = obj["bases"]["controller"];
        bases.scope_mux = obj["bases"]["scope_mux"];
        bases.hil_control = obj["bases"]["hil_control"];


        offsets.cores_rom = obj["offsets"]["cores_rom"];
        offsets.dma = obj["offsets"]["dma"];
        offsets.cores_control = obj["offsets"]["cores_control"];
        offsets.controller = obj["offsets"]["controller"];
        offsets.cores_inputs = obj["offsets"]["cores_inputs"];
        offsets.hil_tb = obj["offsets"]["hil_tb"];
    };

    nlohmann::json dump_layout_object(){
        nlohmann::json ret;
        ret["offsets"] = nlohmann::json();
        ret["offsets"]["cores_rom"] = offsets.cores_rom;
        ret["offsets"]["cores_control"] = offsets.cores_control;
        ret["offsets"]["controller"] = offsets.controller;
        ret["offsets"]["cores_inputs"] = offsets.cores_inputs;
        ret["offsets"]["dma"] = offsets.dma;
        ret["offsets"]["hil_tb"] = offsets.hil_tb;

        ret["bases"] = nlohmann::json();
        ret["bases"]["cores_rom"] = bases.cores_rom;
        ret["bases"]["cores_control"] = bases.cores_control;
        ret["bases"]["cores_inputs"] = bases.cores_inputs;
        ret["bases"]["controller"] = bases.controller;
        ret["bases"]["scope_mux"] = bases.scope_mux;
        ret["bases"]["hil_control"] = bases.hil_control;
        return ret;
    }

    std::string dump() const{
        std::string ret;
        ret += "CORES ROM: BASE " + std::to_string(bases.cores_rom) +
               ", OFFSET " + std::to_string(offsets.cores_rom) + "\n";
        ret += "CORES CONTROL: BASE " + std::to_string(bases.cores_control) +
               ", OFFSET " + std::to_string(offsets.cores_control) + "\n";
        ret += "CORES INPUTS: BASE " + std::to_string(bases.cores_inputs) +
               ", OFFSET " + std::to_string(offsets.cores_inputs) + "\n";
        ret += "DMA OFFSET: " + std::to_string(offsets.dma) + "\n";
        ret += "CONTROLLER: BASE " + std::to_string(bases.controller) +
               ", OFFSET " + std::to_string(offsets.controller) + "\n";

        ret += "HIL CONTROL: BASE " + std::to_string(bases.hil_control) + "\n";
        ret += "SCOPE MUX: BASE " + std::to_string(bases.scope_mux) + "\n";
        ret += "HIL TB: OFFSET " + std::to_string(offsets.hil_tb) + "\n";
        return ret;
    }
    logic_layout_bases bases;
    logic_layout_offsets offsets;
};



struct input_metadata_t{
    std::string core;
    uint64_t const_ip_addr;
    uint32_t dest;
    bool is_float;
};

class deployer_base {
public:
    deployer_base(std::shared_ptr<fpga_bridge>  &h);

    void set_layout_map(nlohmann::json &obj){
        spdlog::info("SETUP HIL ADDRESS MAP");
        addresses.parse_layout_object(obj);
        spdlog::trace(addresses.dump());
    };

    nlohmann::json get_layout_map(){
        return addresses.dump_layout_object();
    };

protected:
    std::pair<uint16_t, uint16_t> get_bus_address(const output_specs_t& spec){return bus_map.translate_output(spec);};

    void setup_base(const fcore::emulator::emulator_specs &specs);
    void write_register(uint64_t addr, uint32_t val);
    void load_core(uint64_t address, const std::vector<uint32_t> &program);
    void setup_core(uint64_t core_address, uint32_t n_channels);
    void setup_memories(uint64_t address, const std::vector<fcore::emulator::emulator_memory_specs> &init_val);

    void setup_inputs(const fcore::emulator::emulator_core &c, uint64_t complex_address, uint64_t inputs_offset, uint64_t const_offset);

    uint16_t setup_output_dma(uint64_t address, const std::string& core_name);
    void setup_output_entry(const bus_map_entry &e, uint64_t dma_address, uint32_t io_progressive);

    static uint32_t get_metadata_value(uint8_t size, bool is_signed, bool is_float);

    void update_input_value(uint32_t address, uint32_t value, std::string core);

    logic_layout addresses;
private:
    hil_bus_map bus_map;


    std::vector<input_metadata_t> inputs;
    std::shared_ptr<fpga_bridge> hw;
};


#endif //USCOPE_DRIVER_DEPLOYER_BASE_HPP
