

//   Copyright 2024 Filippo Savi <filssavi@gmail.com>
//
//   Licensed under the Apache License, Version 2.0 (the "License");allocation_map
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

#include "hil/hil_deployer.hpp"

hil_deployer::hil_deployer(std::shared_ptr<fpga_bridge> &h) {
    hw = h;
}

void hil_deployer::deploy(nlohmann::json &spec) {

    std::string s_f = SCHEMAS_FOLDER;
    try{
        fcore_toolchain::schema_validator_base validator(s_f + "/emulator_spec_schema.json");
        validator.validate(spec);
    } catch(std::invalid_argument &ex){
        throw std::runtime_error(ex.what());
    }
    emulator_manager em(spec, false, s_f);
    auto programs = em.get_programs();
    auto interconnects = em.load_interconnects(spec["interconnect"]);

    hil_bus_map output_bus_map;
    hil_bus_map memory_bus_map;
    // TODO: THERE ARE CONNECTIONS THAT GO FROM A MEMORY CELL TO AN INPUT, IN THIS CASE I NEED TO DMA OUT THE MEMORY AS WELL
    for(auto &p:programs){
        for(auto &io:p.io){
            if(io.type == "o"){
                bus_map_entry e;
                e.core_name = p.name;
                e.bus_address = get_free_address(io.io_addr, p.name);
                e.io_address = io.io_addr;
                e.type = io.type;
                output_bus_map.push_back(e);
            } else if (io.type == "m"){
                bus_map_entry e;
                e.core_name = p.name;
                e.bus_address = get_free_address(io.io_addr, p.name);
                e.io_address = io.io_addr;
                e.type = io.type;
                memory_bus_map.push_back(e);
            }
        }
    }

    std::cout << "------------------------------------------------------------------"<<std::endl;
    for(int i = 0; i<programs.size(); i++){
        std::cout<<"SETUP PROGRAM FOR CORE: "<<programs[i].name <<" AT ADDRESS: "<< to_hex(get_core_address(i)) <<std::endl;
        load_core(get_core_address(i), programs[i].program);
    }
    std::cout << "------------------------------------------------------------------"<<std::endl;




    hil_bus_map input_bus_map;
    for(auto &i:interconnects){
        for(auto &c:i.connections){
            if(auto item = output_bus_map.at_bus(c.first.address, i.source)){
                // Nothing to do in this case
                continue;
            } else if(auto item = memory_bus_map.at_bus(c.first.address, i.source)){
                // Nothing to do in this case
                bus_map_entry e = item.value();
                e.type = "o";
                output_bus_map.push_back(e);
                continue;
            } else{
                std::cout<< "WARNING: Input remapping is not currently supported, thus the io address space must be de-conflicted manually"<< std::endl;
            }
        }

    }

    for(int i = 0; i<programs.size(); i++){
        setup_output_dma(get_dma_address(i), output_bus_map, programs[i].io, programs[i].name);
    }
}

uint16_t hil_deployer::get_free_address(uint16_t original_addr, const std::string &c_n) {
    if(!bus_address_index.contains(original_addr)){
        bus_address_index.insert({original_addr, {c_n, original_addr}});
        return original_addr;
    }

    for(uint16_t i = 0; i<bus_address_index.size(); i++){
        if(!bus_address_index.contains(i)){
            bus_address_index.insert({i, {c_n, original_addr}});
            return i;
        }
    }
    uint16_t idx = bus_address_index.size()+1;
    bus_address_index.insert({idx, {c_n, original_addr}});
    return idx;
}

uint64_t hil_deployer::get_core_address(uint16_t core_address) const {
    return cores_base_address +  core_address*cores_offset;
}

uint64_t hil_deployer::get_dma_address(uint16_t dma_address) const {
    return dma_base_address + dma_address*dma_offset;
}

void hil_deployer::set_cores_location(uint64_t base, uint64_t offset) {
    cores_base_address = base;
    cores_offset = offset;
}

void hil_deployer::set_dma_location(uint64_t base, uint64_t offset) {
    dma_base_address = base;
    dma_offset = offset;
}

void hil_deployer::load_core(uint64_t address, const std::vector<uint32_t> &program) {
    hw->apply_program(address, program);
}

void hil_deployer::setup_output_dma(uint64_t address, hil_bus_map &bus_map, std::set<io_map_entry> io_map, std::string core_name) {
    std::cout<<"SETUP DMA FOR CORE: "<<core_name<<" AT ADDRESS: "<< to_hex(address) <<std::endl;
    std::cout << "------------------------------------------------------------------"<<std::endl;
    int current_io = 0;
    for(auto &i:io_map){
        if(i.type == "o"){
           if(auto entry = bus_map.at_io(i.io_addr, core_name)){
               setup_output_entry(i.io_addr, entry->bus_address, address, current_io);
               current_io++;
           } else{
               throw std::runtime_error("INTERNAL ERROR: unable to setup dma addresses");
           }
        }else if(i.type == "m"){
            if(auto entry = bus_map.at_io(i.io_addr, core_name)){
                setup_output_entry(i.io_addr, entry->bus_address, address, current_io);
                current_io++;
            }
        }
    }
    write_register(address, current_io+1);
    std::cout << "------------------------------------------------------------------"<<std::endl;
}

uint32_t hil_deployer::pack_address_mapping(uint16_t upper, uint16_t lower) const {
    return (upper<<16) | lower;
}

std::string hil_deployer::to_hex(uint64_t i) const {
    return std::format("0x{:x}", i);
}

void hil_deployer::write_register(uint64_t addr, uint32_t val) {
    std::cout<< "write " << to_hex(val) << " to address "<<to_hex(addr)<<std::endl;
    nlohmann::json write;
    write["type"] = "direct";
    write["address"] = addr;
    write["value"] = val;

    hw->single_write_register(write);
}

void hil_deployer::setup_output_entry(uint16_t io_addr, uint16_t bus_address, uint64_t dma_address, uint32_t io_progressive) {
    uint32_t mapping = pack_address_mapping(bus_address, io_addr);
    uint64_t current_address = dma_address + 4 + io_progressive*4;
    std::cout<< "map core io address: " << std::to_string(io_addr) << " to hil bus address "<< std::to_string(bus_address)<<std::endl;
    write_register(current_address, mapping);
}


