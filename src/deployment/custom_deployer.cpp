

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



responses::response_code custom_deployer::deploy(nlohmann::json &arguments) {

    if(runtime_config.debug_hil) dispatcher.enable_debug_mode();
    dispatcher.set_specs(arguments);
    auto programs = dispatcher.get_programs();


    bus_map.clear();
    bus_map.set_map(dispatcher.get_interconnect_slots());
    bus_map.check_conflicts();


    spdlog::info("------------------------------------------------------------------");
    for(auto &p: programs){
        uint64_t core_address = 0;
        core_address = dispatcher.get_deployment_options(p.name).rom_address;
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
        if(n_transfers > max_transfers) max_transfers = n_transfers;
    }


    auto memories = dispatcher.get_memory_initializations();

    for(auto &p: programs){
        spdlog::info("SETUP INITIAL STATE FOR CORE: {0}", p.name);
        uint64_t control_address = 0;
        control_address = dispatcher.get_deployment_options(p.name).control_address;
        this->setup_memories(control_address, memories[p.name], p.n_channels);
    }


    for(auto &p:programs){
        auto inputs = dispatcher.get_inputs(p.name);
        spdlog::info("SETUP INPUTS FOR CORE: {0}", p.name);
        spdlog::info("------------------------------------------------------------------");
        for(int i = 0; i<inputs.size(); i++)  {
            uint64_t complex_base_addr = 1; // this->addresses.bases.cores_control + this->addresses.offsets.cores_control*p.index;

            auto in = inputs[i];
            uint64_t ip_addr;
            if(in.source_type==fcore::random_input) {
                ip_addr = complex_base_addr + this->addresses.bases.noise_generator;
            } else {
                ip_addr = complex_base_addr + this->addresses.bases.cores_inputs;
            }

            if(inputs[i].data.size()>1 && p.n_channels >1) {
                for(int j = 0; j<inputs[i].data.size(); j++) {
                    this->setup_input(
                        in,
                        ip_addr,
                        i,
                        j,
                        p.name + "[" + std::to_string(j)  + "]"
                    );
                    in.data.erase(in.data.begin());
                }
            } else {
                this->setup_input(
                        in,
                        ip_addr,
                        i,
                        0,
                        p.name
                );
            }
        }
        spdlog::info("------------------------------------------------------------------");

    }

    for(auto &p:programs){
        auto opts = dispatcher.get_deployment_options(p.name);
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


