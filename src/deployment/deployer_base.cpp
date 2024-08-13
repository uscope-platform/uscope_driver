

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

#include "deployment/deployer_base.hpp"

deployer_base::deployer_base(std::shared_ptr<fpga_bridge> &h) {
    hw = h;
}

void deployer_base::write_register(uint64_t addr, uint32_t val) {
    spdlog::info("write 0x{0:x} to address {1:x}", val, addr);
    nlohmann::json write;
    write["type"] = "direct";
    write["address"] = addr;
    write["value"] = val;

    hw->single_write_register(write);
}

void deployer_base::load_core(uint64_t address, const std::vector<uint32_t> &program) {
    hw->apply_program(address, program);
}

uint16_t deployer_base::setup_output_dma(uint64_t address, const std::string &core_name) {
    spdlog::info("SETUP DMA FOR CORE: {0} AT ADDRESS 0x{1:x}", core_name, address);
    spdlog::info("------------------------------------------------------------------");
    int current_io = 0;
    for(auto &i:bus_map){
        if(i.core_name == core_name){
            setup_output_entry(i, address, current_io);
            current_io++;
        }
    }
    write_register(address, current_io);
    spdlog::info("------------------------------------------------------------------");
    return current_io;
}

void deployer_base::setup_output_entry(const bus_map_entry &e, uint64_t dma_address, uint32_t io_progressive) {

    if(e.source_io_address > 0xfff){
        throw std::runtime_error("The maximum source address in an interconnect is 0xFFF");
    }
    if(e.destination_bus_address > 0xfff){
        throw std::runtime_error("The maximum destination address in an interconnect is 0xFFF");
    }
    if(e.source_channel > 0xF){
        throw std::runtime_error("The maximum number source of channels in an interconnect is 16");
    }
    if(e.destination_channel > 0xF){
        throw std::runtime_error("The maximum number destination of channels in an interconnect is 16");
    }


    uint16_t source_portion = (e.source_io_address & 0xFFF) |((e.source_channel & 0xF)<<12);
    uint16_t destination_portion = (e.destination_bus_address & 0xFFF) |((e.destination_channel & 0xF)<<12);

    uint32_t mapping = (destination_portion<<16) | source_portion;

    uint64_t mapping_address = dma_address + 4 + io_progressive*4;
    spdlog::info("map core io address: ({0},{1}) to hil bus address: ({2},{3})",
                 e.source_io_address,e.source_channel, e.destination_bus_address, e.destination_channel);
    write_register(mapping_address, mapping);

    auto n_dma_channels = 16;

    uint64_t metadata_address = dma_address + 4*(n_dma_channels + 1) + io_progressive*4;

    write_register(metadata_address, get_metadata_value(32, false, true));
}

uint32_t deployer_base::get_metadata_value(uint8_t size, bool is_signed, bool is_float) {
    uint32_t ret = size-8;
    ret = is_signed ? (ret | 0x10): ret;
    ret = is_float  ?( ret | 0x20 ): ret;

    return ret;
}

void deployer_base::setup_core(uint64_t core_address, uint32_t n_channels) {
    write_register(core_address, n_channels);
}

void
deployer_base::setup_memories(uint64_t address, const std::vector<fcore::emulator::emulator_memory_specs> &init_val) {
    spdlog::info("------------------------------------------------------------------");
    for(auto &i:init_val){

        if(std::holds_alternative<std::vector<float>>(i.value)){
            auto values = std::get<std::vector<float>>(i.value);
            write_register(address+i.address[0]*4, fcore::emulator_backend::float_to_uint32(values[0]));
        } else {
            auto values = std::get<std::vector<uint32_t>>(i.value);
            write_register(address+i.address[0]*4, values[0]);
        }
    }
    spdlog::info("------------------------------------------------------------------");
}
