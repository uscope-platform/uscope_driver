

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
    spdlog::info("HIL CLOCK FREQUENCY: {0}", hil_clock_frequency);
}

responses::response_code hil_deployer::deploy(nlohmann::json &spec) {
    std::string s_f = SCHEMAS_FOLDER;

    //cleanup bus map from previous deployment data;
    bus_map.clear();

    fcore::emulator_manager em(spec, runtime_config.debug_hil, s_f);
    auto programs = em.get_programs();
    auto specs = fcore::emulator::emulator_specs(spec,s_f + "/emulator_spec_schema.json");

    if(programs.size()>32){
        auto msg = "The HIL SYSTEM ONLY SUPPORTS UP TO 32 CORES AT ONCE";
        spdlog::critical(msg);
        throw std::runtime_error(msg);
    }

    reserve_inputs(specs.interconnects);
    reserve_outputs(specs.cores, programs);

    bus_map.check_conflicts();

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
        load_core(get_core_rom_address(i), programs[i].program.binary);
        check_reciprocal(programs[i].program.binary);
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
        setup_initial_state(get_core_control_address(i), programs[i].memories);
    }


    for(int i = 0; i<programs.size(); i++){
        setup_inputs(spec["cores"][i]["id"], spec["cores"][i]["inputs"]);
    }

    setup_sequencer(programs.size(), dividers, shifts);
    setup_cores(programs.size());

    return responses::ok;
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
            setup_output_entry(i, address, current_io);
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

void hil_deployer::setup_output_entry(const bus_map_entry &e, uint64_t dma_address, uint32_t io_progressive) {

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
    uint64_t current_address = dma_address + 4 + io_progressive*4;
    spdlog::info("map core io address: ({0},{1}) to hil bus address: ({2},{3})",
                 e.source_io_address,e.source_channel, e.destination_bus_address, e.destination_channel);

    write_register(current_address, mapping);
}

void hil_deployer::setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& shifts) {
    spdlog::info("SETUP SEQUENCER");
    spdlog::info("------------------------------------------------------------------");

    auto timebase_reg_val = (uint32_t)(hil_clock_frequency/timebase_frequency);


    std::bitset<32> enable;
    for(int i = 0; i<n_cores; i++){
        enable[i] = true;
        write_register(controller_base + controller_offset + 0x4 + 4 * i, divisors[i]-1);
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

void hil_deployer::reserve_inputs(std::vector<fcore::emulator::emulator_interconnect> &ic) {
    hil_bus_map input_bus_map;
    for(auto &i:ic){
        for(auto &c:i.channels){
            bus_map.add_interconnect_channel(c, i.source_core_id, i.destination_core_id);
        }
    }

}

void hil_deployer::reserve_outputs(std::vector<fcore::emulator::emulator_core> &cores, std::vector<fcore::program_bundle> &programs) {
    for(const auto &c:cores){

        bus_map.add_standalone_output(c);
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

void hil_deployer::setup_initial_state(uint64_t address, const std::vector<fcore::emulator::emulator_memory_specs> &init_val) {
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

void hil_deployer::select_output(uint32_t channel, const output_specs_t& output) {
    spdlog::info("HIL SELECT OUTPUT: selected output {0}({1},{2}) from core {3} for scope channel {4}",
                 output.source_output,
                 output.address,
                 output.channel,
                 output.core_name,
                 channel);

    auto data =bus_map.translate_output(output);
    auto selector = data.first | (data.second <<16);
    write_register(scope_mux_base + 4*channel+ 4, selector);
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
                input_value = fcore::emulator_backend::float_to_uint32(in_specs[i]["source"]["value"][0]);
                spdlog::info("set default value {0} for input {1} at address {2} on core {3}",input_value, in_name, address, core);

            } else {
                input_value = in_specs[i]["source"]["value"][0];
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
        double total_length = (p.program.program_length.fixed_portion + n_c[i]*p.program.program_length.per_channel_portion)*clock_period;

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
        auto l = programs[i].program.program_length.fixed_portion + n_c[i]*programs[i].program.program_length.per_channel_portion;
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
