

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

custom_deployer::custom_deployer(std::shared_ptr<fpga_bridge> &h) : deployer_base(h) {

    auto clock_f = std::getenv("HIL_CLOCK_FREQ");

    if(clock_f != nullptr){
        clock_frequency = std::stof(clock_f);
    }
    spdlog::info("CUSTOM LOGIC CLOCK FREQUENCY: {0}", clock_frequency);
}

responses::response_code custom_deployer::deploy(fcore::emulator::emulator_specs &specs, const std::vector<fcore::program_bundle> &programs) {
    std::string s_f = SCHEMAS_FOLDER;


    setup_base(specs);


    spdlog::info("------------------------------------------------------------------");
    for(int i = 0; i<programs.size(); i++){
        auto core_address = specs.cores[i].deployment.rom_address;
        spdlog::info("SETUP PROGRAM FOR CORE: {0} AT ADDRESS: 0x{1:x}", specs.cores[i].id, core_address);
        cores_idx[specs.cores[i].id] = i;
        load_core(core_address, programs[i].program.binary);
    }


    uint32_t complex_base_to_inputs_offset = addresses.bases.cores_inputs;
    uint32_t input_to_input_offset = addresses.offsets.cores_inputs;
    uint32_t complex_base_to_dma_offset = 0x0;
    uint32_t dma_channel_to_channel_offset = 0x0;

    uint16_t max_transfers = 0;
    for(int i = 0; i<programs.size(); i++){

        auto  dma_address = complex_base_to_dma_offset + i*dma_channel_to_channel_offset;
        auto n_transfers = setup_output_dma(dma_address, specs.cores[i].id);
        if(n_transfers > max_transfers) max_transfers = n_transfers;
    }

    for(int i = 0; i<programs.size(); i++){
        spdlog::info("SETUP INITIAL STATE FOR CORE: {0}", specs.cores[i].id);
        auto control_address = specs.cores[i].deployment.control_address;
        setup_memories(control_address, programs[i].memories);
    }


    for(auto &c: specs.cores){
        uint64_t complex_base_addr = c.deployment.control_address;
        setup_inputs(
                c,
                complex_base_addr,
                complex_base_to_inputs_offset,
                input_to_input_offset
        );
    }

    for(auto &c:specs.cores){
        setup_core(c.deployment.control_address, c.channels);
    }


    return responses::ok;
}
