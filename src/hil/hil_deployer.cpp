

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
    full_cores_override = true; // ONLY FULL CORES ARE USED RIGHT NOW


    auto clock_f = std::getenv("HIL_CLOCK_FREQ");
    if(clock_f != nullptr){
        hil_clock_frequency = std::stof(clock_f);
    }
}

responses::response_code hil_deployer::deploy(nlohmann::json &spec) {
    std::string s_f = SCHEMAS_FOLDER;
    try{
        fcore::schema_validator_base validator(s_f + "/emulator_spec_schema.json");
        validator.validate(spec);
    } catch(std::invalid_argument &ex){
        throw std::runtime_error(ex.what());
    }
    fcore::emulator_manager em(spec, runtime_config.debug_hil, s_f);
    auto programs = em.get_programs();
    auto interconnects = em.load_interconnects(spec["interconnect"]);

    if(programs.size()>32){
        auto msg = "The HIL SYSTEM ONLY SUPPORTS UP TO 32 CORES AT ONCE";
        spdlog::critical(msg);
        throw std::runtime_error(msg);
    }

    reserve_inputs(interconnects);
    reserve_outputs(programs);


    std::vector<uint32_t> frequencies;

    for(auto & program : programs){
        frequencies.push_back(program.sampling_frequency);
    }

    timebase_frequency = std::accumulate(frequencies.begin(), frequencies.end(), 1,[](uint32_t a, uint32_t b){
        return std::lcm(a,b);
    });

    spdlog::info("------------------------------------------------------------------");
    for(int i = 0; i<programs.size(); i++){
        spdlog::info("SETUP PROGRAM FOR CORE: {0} AT ADDRESS: 0x{1:x}", programs[i].name, get_core_rom_address(i));
        cores_idx[programs[i].name] = i;
        load_core(get_core_rom_address(i), programs[i].program);
        check_reciprocal(programs[i].program);
    }

    auto dividers = calculate_timebase_divider(programs, n_channels);
    auto shifts = calculate_timebase_shift(programs, n_channels);

    spdlog::info("------------------------------------------------------------------");

    uint16_t max_transfers = 0;
    for(int i = 0; i<programs.size(); i++){
        auto n_transfers = setup_output_dma(get_dma_address(i), programs[i].name);
        if(n_transfers > max_transfers) max_transfers = n_transfers;
    }

    for(int i = 0; i<programs.size(); i++){
        spdlog::info("SETUP INITIAL STATE FOR CORE: {0}", programs[i].name);
        setup_initial_state(get_core_control_address(i), programs[i].mem_init);
    }


    for(int i = 0; i<programs.size(); i++){
        setup_inputs(spec["cores"][i]["id"], spec["cores"][i]["inputs"]);
    }

    setup_sequencer(programs.size(), dividers, shifts);
    setup_cores(programs.size());

    //cleanup leftovers from deployment process
    bus_map.clear();
    bus_address_index.clear();
    return responses::ok;
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



void hil_deployer::load_core(uint64_t address, const std::vector<uint32_t> &program) {
    hw->apply_program(address, program);
}

uint16_t hil_deployer::setup_output_dma(uint64_t address, const std::string& core_name) {
    spdlog::info("SETUP DMA FOR CORE: {0} AT ADDRESS 0x{1:x}", core_name, address);
    spdlog::info("------------------------------------------------------------------");
    int current_io = 0;
    for(auto &i:bus_map){
        if(i.core_name == core_name){
            setup_output_entry(i.io_address, i.bus_address, address, current_io);
            current_io++;
        }
    }
    write_register(address, current_io);
    spdlog::info("------------------------------------------------------------------");
    return current_io;
}

uint32_t hil_deployer::pack_address_mapping(uint16_t upper, uint16_t lower) const {
    return (upper<<16) | lower;
}


void hil_deployer::write_register(uint64_t addr, uint32_t val) {
    spdlog::info("write 0x{0:x} to address {1:x}", val, addr);
    nlohmann::json write;
    write["type"] = "direct";
    write["address"] = addr;
    write["value"] = val;

    hw->single_write_register(write);
}

void hil_deployer::setup_output_entry(uint16_t io_addr, uint16_t bus_address, uint64_t dma_address, uint32_t io_progressive) {
    uint32_t mapping = pack_address_mapping(bus_address, io_addr);
    uint64_t current_address = dma_address + 4 + io_progressive*4;
    spdlog::info("map core io address: {0} to hil bus address {1}", io_addr, bus_address);
    write_register(current_address, mapping);
}

void hil_deployer::setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& shifts) {
    spdlog::info("SETUP SEQUENCER");
    spdlog::info("------------------------------------------------------------------");

    auto timebase_reg_val = (uint32_t)(hil_clock_frequency/timebase_frequency);


    std::bitset<32> enable;
    for(int i = 0; i<n_cores; i++){
        enable[i] = true;
        auto register_setting = divisors[i]-1;
        write_register(controller_base + controller_offset + 0x4 + 4 * i, register_setting);
        write_register(controller_base + hil_tb_offset + 8 + 4*i, shifts[i]);
    }

    write_register(controller_base + hil_tb_offset + 4, timebase_reg_val);

    write_register(controller_base + controller_offset, enable.to_ulong());


    spdlog::info("------------------------------------------------------------------");
}

void hil_deployer::setup_cores(uint16_t n_cores) {
    spdlog::info("SETUP CORES");
    spdlog::info("------------------------------------------------------------------");
    for(int i = 0; i<n_cores; i++){
        write_register(get_core_control_address(i), n_channels[i]);
    }
}

void hil_deployer::reserve_inputs(std::vector<fcore::interconnect_t> &ic) {
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
                spdlog::warn("WARNING: Unsolvable input bus address conflict detected");
            }
        }
    }

}

void hil_deployer::reserve_outputs(std::vector<fcore::program_bundle> &programs) {
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
    int section=0;
    bool skip_next = false;
    for(auto &instr:program){
        if(section <2){
            if(instr==0xC) section++;
        } else {
            if(skip_next){
                skip_next = false;
                continue;
            } if((instr & ((1<<fcore::fcore_opcode_width)-1)) == fcore::fcore_opcodes["ldc"]){
                skip_next = true;
            }if((instr & ((1<<fcore::fcore_opcode_width)-1)) == fcore::fcore_opcodes["rec"]) {
                rec_present = true;
            }
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
    spdlog::info("------------------------------------------------------------------");
    for(auto &i:init_val){
        write_register(address+i.first*4, i.second);
    }
    spdlog::info("------------------------------------------------------------------");
}

void hil_deployer::select_output(uint32_t channel, uint32_t address) {
    spdlog::info("HIL SELECT OUTPUT: selected output {0} for channel {1}", address, channel);write_register(scope_mux_base + 4*channel+ 4, address);
}

void hil_deployer::set_input(uint32_t address, uint32_t value, std::string core) {
    spdlog::info("HIL SET INPUT: set value {0} for input at address {1}, on core {2}", value, address, core);
    for(auto &in:inputs){
        if(in.dest == address && in.core == core){
            write_register( in.const_ip_addr+ 8, address);
            write_register( in.const_ip_addr, value);
        }
    }

}

void hil_deployer::setup_inputs(const std::string &core, nlohmann::json &in_specs) {
    spdlog::info("SETUP INPUTS FOR CORE: {0}", core);
    spdlog::info("------------------------------------------------------------------");
    for(int i = 0; i<in_specs.size(); ++i){
        if(in_specs[i]["source"]["type"] == "constant"){
            std::string in_name = in_specs[i]["name"];
            uint32_t address =in_specs[i]["reg_n"];

            uint64_t complex_base_addr =cores_control_base_address + cores_control_offset*cores_idx[core];
            uint64_t offset = cores_inputs_base_address + i*cores_inputs_offset;

            input_metadata_t in;

            uint32_t input_value;
            in.is_float = in_specs[i]["type"] == "float";
            in.core = core;
            in.const_ip_addr = complex_base_addr + offset;
            in.dest = address;

            if(in.is_float){
                input_value = fcore::emulator::float_to_uint32(in_specs[i]["source"]["value"]);
                spdlog::info("set default value {0} for input {1} at address {2} on core {3}",input_value, in_name, address, core);

            } else {
                input_value = in_specs[i]["source"]["value"];
                spdlog::info("set default value {0} for input {1} at address {2} on core {3}",input_value, in_name, address, core);
            }

            write_register( in.const_ip_addr+ 8, address);
            write_register( in.const_ip_addr, input_value);

            inputs.push_back(in);
        }

    }
    spdlog::info("------------------------------------------------------------------");
}

void hil_deployer::start() {
    spdlog::info("START HIL");
    write_register(hil_control_base, 1);
}

void hil_deployer::stop() {
    spdlog::info("STOP HIL");
    write_register(hil_control_base, 0);
}

std::vector<uint32_t> hil_deployer::calculate_timebase_divider(const std::vector<fcore::program_bundle> &programs, std::vector<uint32_t> n_c) {
    std::vector<uint32_t>core_dividers;

    for(int i = 0; i<programs.size(); i++){
        auto p = programs[i];
        double clock_period = 1/hil_clock_frequency;
        double total_length = (p.program_length.fixed_portion + n_c[i]*p.program_length.per_channel_portion)*clock_period;

        double max_frequency = 1/total_length;

        while(timebase_frequency/timebase_divider > max_frequency){
            timebase_divider += 1.0;
        }

        core_dividers.push_back((uint32_t) timebase_frequency/p.sampling_frequency);
    }

    return core_dividers;

}

std::vector<uint32_t>
hil_deployer::calculate_timebase_shift(const std::vector<fcore::program_bundle> &programs, std::vector<uint32_t> n_c) {

    struct shift_calc_data {
        std::string name;
        uint32_t execution_order;
        uint32_t program_length;
    };

    std::vector<shift_calc_data> calc_data_v;
    for(int i = 0; i<programs.size(); i++){
        auto l = programs[i].program_length.fixed_portion + n_c[i]*programs[i].program_length.per_channel_portion;
        calc_data_v.emplace_back(programs[i].name, programs[i].execution_order, l);
    }


    std::ranges::sort(calc_data_v, [](const shift_calc_data &a, const shift_calc_data &b){
        return a.execution_order < b.execution_order;
    });

    uint32_t inter_core_buffer = 60;

    std::unordered_map<std::string, uint32_t>ordered_shifts;
    ordered_shifts[calc_data_v[0].name] = 2;

    uint32_t  next_starting_point = calc_data_v[0].program_length + 2 + inter_core_buffer;

    for(int i = 1; i<calc_data_v.size(); i++){
        ordered_shifts[calc_data_v[i].name] = next_starting_point;
        next_starting_point += calc_data_v[i].program_length + inter_core_buffer;
    }
    std::vector<uint32_t> shifts;

    for(const auto &item:programs){
        shifts.push_back(ordered_shifts[item.name]);
    }

    return shifts;
}
