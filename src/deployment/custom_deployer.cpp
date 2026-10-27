

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

#include "deployment/custom_deployer.hpp"

custom_deployer::custom_deployer() {
    auto clock_f = std::getenv("HIL_CLOCK_FREQ");

    if(clock_f != nullptr){
        clock_frequency = std::stof(clock_f);
    }
    spdlog::info("CUSTOM LOGIC CLOCK FREQUENCY: {0}", clock_frequency);
}



fcore::result<responses::response_code> custom_deployer::deploy(const nlohmann::json &arguments) {
    return fcore::catching([&]{ return run_deployment(arguments); });
}

fcore::result<responses::response_code> custom_deployer::run_deployment(const nlohmann::json &arguments) {

    if(runtime_config.debug_hil) {
        auto debug = dispatcher.enable_debug_mode();
        if(!debug) return fcore::fail(std::move(debug).error());
    }
    auto specs = dispatcher.set_specs(arguments);
    if(!specs) return fcore::fail(std::move(specs).error());
    auto programs_res = dispatcher.get_programs();
    if(!programs_res) return fcore::fail(std::move(programs_res).error());
    auto &programs = *programs_res;


    bus_map.clear();
    auto slots = dispatcher.get_interconnect_slots();
    if(!slots) return fcore::fail(std::move(slots).error());
    bus_map.set_map(*slots);

    spdlog::info("------------------------------------------------------------------");
    for(auto &p: programs){
        auto opts = dispatcher.get_deployment_options(p.name);
        if(!opts) return fcore::fail(std::move(opts).error());
        uint64_t core_address = opts->rom_address;
        spdlog::info("SETUP PROGRAM FOR CORE: {0} AT ADDRESS: 0x{1:x}", p.name, core_address);
        cores_idx[p.name] = p.index;
        this->load_core(core_address, p.program.binary);
    }

    spdlog::info("------------------------------------------------------------------");

    uint16_t max_transfers = 0;
    for(int i = 0; i<programs.size(); i++){

        uint64_t complex_base_addr = this->addresses.bases.cores_control + this->addresses.offsets.cores_control*i;
        auto  dma_address = complex_base_addr + this->addresses.offsets.dma;

        auto n_transfers = this->setup_output_dma(dma_address, programs[i].name, programs[i].n_channels);
        if(!n_transfers) return fcore::fail(std::move(n_transfers).error().with_context("setting up the output DMA of core " + programs[i].name));
        if(*n_transfers > max_transfers) max_transfers = *n_transfers;
    }


    auto memories = dispatcher.get_memory_initializations();
    if(!memories) return fcore::fail(std::move(memories).error());

    for(auto &p: programs){
        spdlog::info("SETUP INITIAL STATE FOR CORE: {0}", p.name);
        auto opts = dispatcher.get_deployment_options(p.name);
        if(!opts) return fcore::fail(std::move(opts).error());
        this->setup_memories(opts->control_address, (*memories)[p.name], p.n_channels);
    }


    for(auto &p:programs){
        auto core_inputs = dispatcher.get_inputs(p.name);
        if(!core_inputs) return fcore::fail(std::move(core_inputs).error());
        auto &inputs = *core_inputs;
        spdlog::info("SETUP INPUTS FOR CORE: {0}", p.name);
        spdlog::info("------------------------------------------------------------------");
        for(int i = 0; i<inputs.size(); i++)  {
            uint64_t complex_base_addr = 1; // this->addresses.bases.cores_control + this->addresses.offsets.cores_control*p.index;

            uint64_t ip_addr;
            if(inputs[i].source_type==fcore::random_input) {
                ip_addr = complex_base_addr + this->addresses.bases.noise_generator;
            } else {
                ip_addr = complex_base_addr + this->addresses.bases.cores_inputs;
            }

            if(inputs[i].data.size()>1 && p.n_channels >1) {
                for(int j = 0; j<inputs[i].data.size(); j++) {
                    if(auto r = this->setup_input(
                        inputs[i],
                        ip_addr,
                        i,
                        j,
                        p.name,
                        true
                    ); !r) return fcore::fail(std::move(r).error());
                    inputs[i].data.erase(inputs[i].data.begin());
                }
            } else {
                if(auto r = this->setup_input(
                        inputs[i],
                        ip_addr,
                        i,
                        0,
                        p.name,
                        false
                ); !r) return fcore::fail(std::move(r).error());
            }
        }
        spdlog::info("------------------------------------------------------------------");

    }

    for(auto &p:programs){
        auto deployment_options = dispatcher.get_deployment_options(p.name);
        if(!deployment_options) return fcore::fail(std::move(deployment_options).error());
        auto &opts = *deployment_options;
        auto min_channels =  opts.has_reciprocal ? 11 : 8;

        if(p.n_channels> min_channels){
            this->setup_core(opts.control_address, p.n_channels);
        } else{
            this->setup_core(opts.control_address, min_channels);
        }
    }


    return responses::ok;
}

void custom_deployer::set_accessor(const std::shared_ptr<bus_accessor> &ba) {
    deployer_base::set_accessor(ba);
}


