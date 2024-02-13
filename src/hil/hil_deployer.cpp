

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
    full_cores_override = true; // FOR NOW ALL CORES ARE COMPILED WITH RECIPROCAL ENABLED
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


    reserve_inputs(interconnects);
    reserve_outputs(programs);

    std::cout << "------------------------------------------------------------------"<<std::endl;
    for(int i = 0; i<programs.size(); i++){
        std::cout<<"SETUP PROGRAM FOR CORE: "<<programs[i].name <<" AT ADDRESS: "<< to_hex(get_core_rom_address(i)) <<std::endl;
        load_core(get_core_rom_address(i), programs[i].program);
        check_reciprocal(programs[i].program);
    }
    std::cout << "------------------------------------------------------------------"<<std::endl;

    uint16_t max_transfers = 0;
    for(int i = 0; i<programs.size(); i++){
        auto n_transfers = setup_output_dma(get_dma_address(i), programs[i].name);
        if(n_transfers > max_transfers) max_transfers = n_transfers;
    }

    for(int i = 0; i<programs.size(); i++){
        std::cout<<"SETUP INITIAL STATE FOR CORE: "<<programs[i].name <<std::endl;
        setup_initial_state(get_core_control_address(i), programs[i].mem_init);
    }

    setup_sequencer(sequencer_address, programs.size(), max_transfers);
    setup_cores(programs.size());

    //cleanup leftovers from deployment process
    bus_map.clear();
    bus_address_index.clear();
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

uint64_t hil_deployer::get_core_rom_address(uint16_t core_address) const {
    return cores_rom_base_address + core_address * cores_rom_offset;
}

uint64_t hil_deployer::get_core_control_address(uint16_t core_idx) const {
    return cores_control_base_address + core_idx * cores_control_offset;
}


uint64_t hil_deployer::get_dma_address(uint16_t dma_address) const {
    return dma_base_address + dma_address*dma_offset;
}

void hil_deployer::set_cores_rom_location(uint64_t base, uint64_t offset) {
    cores_rom_base_address = base;
    cores_rom_offset = offset;
}

void hil_deployer::set_dma_location(uint64_t base, uint64_t offset) {
    dma_base_address = base;
    dma_offset = offset;
}

void hil_deployer::load_core(uint64_t address, const std::vector<uint32_t> &program) {
    hw->apply_program(address, program);
}

uint16_t hil_deployer::setup_output_dma(uint64_t address, const std::string& core_name) {
    std::cout<<"SETUP DMA FOR CORE: "<<core_name<<" AT ADDRESS: "<< to_hex(address) <<std::endl;
    std::cout << "------------------------------------------------------------------"<<std::endl;
    int current_io = 0;
    for(auto &i:bus_map){
        if(i.core_name == core_name){
            setup_output_entry(i.io_address, i.bus_address, address, current_io);
            current_io++;
        }
    }
    write_register(address, current_io);
    std::cout << "------------------------------------------------------------------"<<std::endl;
    return current_io;
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

void hil_deployer::setup_sequencer(uint64_t seq, uint16_t n_cores, uint16_t n_transfers) {
    std::cout<<"SETUP SEQUENCER" <<std::endl;
    std::cout << "------------------------------------------------------------------"<<std::endl;
    write_register(seq, n_cores);
    write_register(seq + 0x4, 0);
    write_register(seq + 0x8, 20*n_transfers);

    for(int i = 0; i<n_cores; i++){
        write_register(seq + 0xC + 4*i, i);
    }

}

void hil_deployer::set_sequencer_location(uint64_t sequencer) {
    sequencer_address = sequencer;
}

void hil_deployer::setup_cores(uint16_t n_cores) {
    std::cout<<"SETUP CORES" <<std::endl;
    std::cout << "------------------------------------------------------------------"<<std::endl;
    for(int i = 0; i<n_cores; i++){
        write_register(get_core_control_address(i), n_channels[i]);
    }

}

void hil_deployer::set_cores_control_location(uint64_t base, uint64_t offset) {
    cores_control_base_address = base;
    cores_control_offset = offset;
}

void hil_deployer::reserve_inputs(std::vector<interconnect_t> &ic) {
    hil_bus_map input_bus_map;
    for(auto &i:ic){
        for(auto &c:i.connections){
            if(!bus_map.has_bus(c.first.address)){
                bus_map_entry e;
                e.core_name = i.source;
                e.bus_address =  c.second.address;
                e.io_address = c.first.address;
                e.type = "o";
                bus_map.push_back(e);
                bus_address_index.insert({(uint16_t) c.second.address, {i.source, c.first.address}});
            } else {
                std::cout << "WARNING: Unsolvable input bus address conflict detected"<<std::endl;
            }
        }
    }

}

void hil_deployer::reserve_outputs(std::vector<program_bundle> &programs) {
    for(auto &p:programs){
        for(auto &io:p.io){
            if(io.type == "o"){
                if(!bus_map.has_io(io.io_addr, p.name)){
                    bus_map_entry e;
                    e.core_name = p.name;
                    e.bus_address = get_free_address(io.io_addr, p.name);
                    e.io_address = io.io_addr;
                    e.type = io.type;
                    bus_map.push_back(e);
                }
            }
        }
    }
}

void hil_deployer::check_reciprocal(const std::vector<uint32_t> &program) {
    bool rec_present = false;
    int section;
    for(auto &instr:program){
        if(section <2){
            if(instr==0xC) section++;
        } else {
            if((instr & ((1<<fcore_opcode_width)-1)) == fcore_opcodes["rec"]) rec_present = true;
        }
    }
    // SET THE NUMBER OF CHANNELS DEPENDING ON THE PIPELINE LENGTH OF THE PROCESSOR TO AVOID WAIT STATES
    if(rec_present | full_cores_override){
        n_channels.push_back(11);
    } else {
        n_channels.push_back(8);
    }
}

void hil_deployer::setup_initial_state(uint64_t address, const std::unordered_map<uint32_t, uint32_t> &init_val) {
    std::cout << "------------------------------------------------------------------"<<std::endl;

    for(auto &i:init_val){
        write_register(address+i.first*4, i.second);
    }
    std::cout << "------------------------------------------------------------------"<<std::endl;
}
