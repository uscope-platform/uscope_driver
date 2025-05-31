

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
#include "../testing/deployment/fpga_bridge_mock.hpp"

custom_deployer::custom_deployer() {
    auto clock_f = std::getenv("HIL_CLOCK_FREQ");

    if(clock_f != nullptr){
        clock_frequency = std::stof(clock_f);
    }
    spdlog::info("CUSTOM LOGIC CLOCK FREQUENCY: {0}", clock_frequency);
}



responses::response_code custom_deployer::deploy(nlohmann::json &arguments) {
    auto specs = fcore::emulator::emulator_specs();
    specs.set_specs(arguments);

    fcore::emulator_dispatcher em;
    if(runtime_config.debug_hil) em.enable_debug_mode();
    em.set_specs(arguments);
    auto programs = em.get_programs();

    this->setup_base(specs);


    spdlog::info("------------------------------------------------------------------");
    for(auto &p: programs){
        uint64_t core_address = 0;
        for(auto &core: specs.cores) {
            if(core.id == p.name) core_address = core.deployment.rom_address;
        }
        spdlog::info("SETUP PROGRAM FOR CORE: {0} AT ADDRESS: 0x{1:x}", p.name, core_address);
        cores_idx[p.name] = p.index;
        this->load_core(core_address, p.program.binary);
    }

    spdlog::info("------------------------------------------------------------------");

    uint16_t max_transfers = 0;
    for(int i = 0; i<programs.size(); i++){

        uint64_t complex_base_addr = this->addresses.bases.cores_control + this->addresses.offsets.cores_control*i;
        auto  dma_address = complex_base_addr + this->addresses.offsets.dma;

        auto n_transfers = this->setup_output_dma(dma_address, specs.cores[i].id);
        if(n_transfers > max_transfers) max_transfers = n_transfers;
    }


    auto memories = em.get_memory_initializations();

    for(auto &p: programs){
        spdlog::info("SETUP INITIAL STATE FOR CORE: {0}", p.name);
        uint64_t control_address = 0;
        for(auto &core: specs.cores) {
            if(core.id == p.name) control_address = core.deployment.control_address;
        }
        this->setup_memories(control_address, memories[p.name]);
    }


    for(auto &c: specs.cores){
        uint64_t complex_base_addr = c.deployment.control_address;
        this->setup_inputs(
                c,
                complex_base_addr,
                this->addresses.bases.cores_inputs,
                this->addresses.offsets.cores_inputs
        );
    }

    for(auto &c:specs.cores){
        auto min_channels = c.deployment.has_reciprocal ? 11 : 8;

        if(c.channels> min_channels){
            this->setup_core(c.deployment.control_address, c.channels);
        } else{
            this->setup_core(c.deployment.control_address, min_channels);
        }
    }


    return responses::ok;
}

void custom_deployer::set_accessor(const std::shared_ptr<bus_accessor> &ba) {
    deployer_base::set_accessor(ba);
}


