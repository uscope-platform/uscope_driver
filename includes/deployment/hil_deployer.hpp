

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

#include "hw_interface/fpga_bridge.hpp"
#include "emulator/emulator_manager.hpp"
#include "fCore_isa.hpp"

#include "deployment/deployer_base.hpp"


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
        ret["bases"]["dma"] = bases.dma;
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
        ret += "DMA: BASE " + std::to_string(bases.dma) +
                ", OFFSET " + std::to_string(offsets.dma) + "\n";
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


class hil_deployer : public deployer_base{
public:
    explicit hil_deployer(std::shared_ptr<fpga_bridge>  &h);

    void set_layout_map(nlohmann::json &obj){
        spdlog::info("SETUP HIL ADDRESS MAP");
        addresses.parse_layout_object(obj);
        spdlog::trace(addresses.dump());
    };

    nlohmann::json get_layout_map(){
        return addresses.dump_layout_object();
    };

    responses::response_code deploy(nlohmann::json &spec);

    void select_output(uint32_t channel, const output_specs_t& output);
    void set_input(uint32_t address, uint32_t value, std::string core);
    void start();
    void stop();
private:


    void setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& orders);
    void setup_cores(uint16_t n_cores);

    void check_reciprocal(const std::vector<uint32_t> &program);
    std::vector<uint32_t> calculate_timebase_divider(const std::vector<fcore::program_bundle> &programs,
                                                                   std::vector<uint32_t> n_c);

    std::vector<uint32_t> calculate_timebase_shift(const std::vector<fcore::program_bundle> &programs,
                                                     std::vector<uint32_t> n_c);


    std::map<std::string, uint32_t> cores_idx;
    float hil_clock_frequency = 100e6;


    logic_layout addresses;

    std::vector<uint32_t> n_channels;

    double timebase_frequency;
    double timebase_divider = 1;

    bool full_cores_override;

};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
