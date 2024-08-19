

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

template<class hw_bridge>
custom_deployer<hw_bridge>::custom_deployer(std::shared_ptr<hw_bridge> &h): deployer_base<hw_bridge>(h) {
    auto clock_f = std::getenv("HIL_CLOCK_FREQ");

    if(clock_f != nullptr){
        clock_frequency = std::stof(clock_f);
    }
    spdlog::info("CUSTOM LOGIC CLOCK FREQUENCY: {0}", clock_frequency);
}

template<class hw_bridge>
responses::response_code custom_deployer<hw_bridge>::deploy(fcore::emulator::emulator_specs &specs, const std::vector<fcore::program_bundle> &programs) {
    std::string s_f = SCHEMAS_FOLDER;


    this->setup_base(specs);


    spdlog::info("------------------------------------------------------------------");
    for(int i = 0; i<programs.size(); i++){
        auto core_address = specs.cores[i].deployment.rom_address;
        spdlog::info("SETUP PROGRAM FOR CORE: {0} AT ADDRESS: 0x{1:x}", specs.cores[i].id, core_address);
        cores_idx[specs.cores[i].id] = i;
        this->load_core(core_address, programs[i].program.binary);
    }

    spdlog::info("------------------------------------------------------------------");

    uint16_t max_transfers = 0;
    for(int i = 0; i<programs.size(); i++){

        uint64_t complex_base_addr = this->addresses.bases.cores_control + this->addresses.offsets.cores_control*i;
        auto  dma_address = complex_base_addr + this->addresses.offsets.dma;

        auto n_transfers = this->setup_output_dma(dma_address, specs.cores[i].id);
        if(n_transfers > max_transfers) max_transfers = n_transfers;
    }

    for(int i = 0; i<programs.size(); i++){
        spdlog::info("SETUP INITIAL STATE FOR CORE: {0}", specs.cores[i].id);
        auto control_address = specs.cores[i].deployment.control_address;
        this->setup_memories(control_address, programs[i].memories);
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


template class custom_deployer<fpga_bridge>;
template class custom_deployer<fpga_bridge_mock>;
