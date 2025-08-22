

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

#include "deployment/hil_deployer.hpp"
#include "../testing/deployment/fpga_bridge_mock.hpp"


hil_deployer::hil_deployer() {
    full_cores_override = true; // ONLY FULL CORES ARE USED RIGHT NOW

    auto clock_f = std::getenv("HIL_CLOCK_FREQ");

    if(clock_f != nullptr){
        hil_clock_frequency = std::stof(clock_f);
    }
    spdlog::info("HIL CLOCK FREQUENCY: {0}", hil_clock_frequency);
}

void hil_deployer::set_accessor(const std::shared_ptr<bus_accessor> &ba) {
    deployer_base::set_accessor(ba);
}

responses::response_code hil_deployer::deploy(nlohmann::json &arguments) {

    min_timebase = 0;
    active_random_inputs = 0;
    inputs_labels.clear();
    bus_labels.clear();

    if(runtime_config.debug_hil) dispatcher.enable_debug_mode();
    dispatcher.set_specs(arguments);
    auto programs = dispatcher.get_programs();

    bus_map.clear();

    bus_map.set_map(dispatcher.get_interconnect_slots());
    bus_map.check_conflicts();


    if(programs.size()>32){
        auto msg = "The HIL SYSTEM ONLY SUPPORTS UP TO 32 CORES AT ONCE";
        spdlog::critical(msg);
        throw std::runtime_error(msg);
    }

    std::vector<uint32_t> frequencies;

    spdlog::info("------------------------------------------------------------------");
    for(auto &p: programs){
        auto core_address = this->addresses.bases.cores_rom + p.index * this->addresses.offsets.cores_rom;
        spdlog::info("SETUP PROGRAM FOR CORE: {0} AT ADDRESS: 0x{1:x}", p.name, core_address);
        cores_idx[p.name] = p.index;
        execution_order[p.name] =  p.order;
        this->load_core(core_address, p.program.binary);
        n_channels[p.name] = check_reciprocal(p.program.binary);
        frequencies.push_back(p.sampling_frequency);

    }
        timebase_frequency = std::accumulate(frequencies.begin(), frequencies.end(), 1,[](uint32_t a, uint32_t b){
        return std::lcm(a,b);
    });

    auto dividers = calculate_timebase_divider();
    auto shifts = calculate_timebase_shift();

    spdlog::info("------------------------------------------------------------------");

    uint16_t max_transfers = 0;
    for(auto &p:programs){
        uint64_t complex_base_addr = this->addresses.bases.cores_control + this->addresses.offsets.cores_control*cores_idx[p.name];
        auto  dma_address = complex_base_addr + this->addresses.offsets.dma;
        auto n_transfers = this->setup_output_dma(dma_address, p.name, p.n_channels);
        if(n_transfers > max_transfers) max_transfers = n_transfers;
    }

    auto memories = dispatcher.get_memory_initializations();


    for(auto &p:programs) {
        auto initializations = memories[p.name];
        spdlog::info("SETUP INITIAL STATE FOR CORE: {0}", p.name);

        auto control_address = this->addresses.bases.cores_control + cores_idx[p.name] * this->addresses.offsets.cores_control;
        this->setup_memories(control_address, initializations, p.n_channels);
    }


    for(auto &p:programs){
        auto inputs = dispatcher.get_inputs(p.name);
        spdlog::info("SETUP INPUTS FOR CORE: {0}", p.name);
        spdlog::info("------------------------------------------------------------------");
        int input_progressive = 0;
        for(auto &input: inputs)  {

            uint64_t complex_base_addr = this->addresses.bases.cores_control + this->addresses.offsets.cores_control*p.index;

            auto in = input;
            uint64_t ip_addr;
            if(in.source_type==fcore::random_input) {
                ip_addr = this->addresses.bases.noise_generator;
            } else {
                ip_addr = complex_base_addr + this->addresses.bases.cores_inputs;
            }
            std::string core_name;
            if(p.n_channels >1) {
                if(input.data.size()>1) {
                    // TODO: test if this support deploying multiple disjointed constants
                    for(int j = 0; j<input.data.size(); j++) {
                        core_name = p.name + "[" + std::to_string(j)  + "]";
                        this->setup_inputs(
                            in,
                            ip_addr,
                            input_progressive,
                            j,
                            core_name
                        );
                        if(inputs_labels.contains(core_name + "." + in.name)) inputs_labels[core_name + "." + in.name].core_idx = p.index;

                        in.data.erase(in.data.begin());
                    }
                } else {
                    if(in.source_type == fcore::random_input) {
                        for(int j = 0; j<p.n_channels; j++) {
                            core_name = p.name + "[" + std::to_string(j)  + "]";
                            this->setup_inputs(
                                in,
                                ip_addr,
                                input_progressive,
                                j,
                                core_name
                            );
                        }
                    }  else if(in.source_type == fcore::constant_input || in.source_type == fcore::time_series_input) {
                        if(input.data.size() == 1) {
                            for(int j = 0; j<p.n_channels; j++) {
                                core_name = p.name + "[" + std::to_string(j)  + "]";
                                this->setup_inputs(
                                    in,
                                    ip_addr,
                                    input_progressive,
                                    j,
                                    core_name
                                );
                                if(inputs_labels.contains(core_name + "." + in.name)) inputs_labels[core_name + "." + in.name].core_idx = p.index;

                            }
                        } else {
                            for(int j = 0; j<input.data.size(); j++) {
                                core_name = p.name + "[" + std::to_string(j)  + "]";
                                this->setup_inputs(
                                    in,
                                    ip_addr,
                                    input_progressive,
                                    j,
                                    core_name
                                );
                                if(inputs_labels.contains(core_name + "." + in.name)) inputs_labels[core_name + "." + in.name].core_idx = p.index;

                                in.data.erase(in.data.begin());
                            }
                        }

                    }
                }

            } else {
                core_name = p.name;
                this->setup_inputs(
                        in,
                        ip_addr,
                        input_progressive,
                        0,
                        core_name
                );
            }
            if(inputs_labels.contains(core_name + "." + in.name)) inputs_labels[core_name + "." + in.name].core_idx = p.index;
            if(in.source_type==fcore::constant_input || in.source_type == fcore::time_series_input) input_progressive++;
        }

        if(active_random_inputs>0) {
            write_register(this->addresses.bases.noise_generator, active_random_inputs);
        }
        spdlog::info("------------------------------------------------------------------");
    }

    setup_sequencer(programs.size(), dividers, shifts);
    setup_cores(programs);

    return responses::ok;
}

void hil_deployer::setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& shifts) {
    spdlog::info("SETUP SEQUENCER");
    spdlog::info("------------------------------------------------------------------");


    std::bitset<32> enable;
    for(int i = 0; i<n_cores; i++){
        enable[i] = true;
        this->write_register(this->addresses.bases.controller + this->addresses.offsets.controller + 0x4 + 4 * i, divisors[i]-1);
        this->write_register(this->addresses.bases.controller + this->addresses.offsets.hil_tb + 8 + 4*i, shifts[i]);
    }

    if(timebase_frequency == 0) {
        this->write_register(this->addresses.bases.controller + this->addresses.offsets.hil_tb + 4, min_timebase);
    } else {
        this->write_register(this->addresses.bases.controller + this->addresses.offsets.hil_tb + 4, hil_clock_frequency/timebase_frequency);
    }


    this->write_register(this->addresses.bases.controller + this->addresses.offsets.controller, enable.to_ulong());


    spdlog::info("------------------------------------------------------------------");
}

void hil_deployer::setup_cores(std::vector<fcore::deployed_program> &programs) {
    spdlog::info("SETUP CORES");
    spdlog::info("------------------------------------------------------------------");
    for(auto p : programs ){
        this->setup_core(this->addresses.bases.cores_control + p.index * this->addresses.offsets.cores_control,  n_channels[p.name]);
    }
}


uint32_t hil_deployer::check_reciprocal(const std::vector<uint32_t> &program) {
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
        return 11;
    } else {
        return 8;
    }
}

void hil_deployer::select_output(uint32_t channel, const output_specs_t& output) {
    spdlog::info("HIL SELECT OUTPUT: selected output {0}({1},{2}) from core {3} for scope channel {4}",
                 output.source_output,
                 output.address,
                 output.channel,
                 output.core_name,
                 channel);

    auto data = this->get_bus_address(output);
    auto selector = data.address | (data.channel <<16);
    this->write_register(this->addresses.bases.scope_mux + 4*channel+ 4, selector);
}

void hil_deployer::set_input(uint32_t address, uint32_t value, std::string core) {
    this->update_input_value(address, value, core);
}

void hil_deployer::start() {
    spdlog::info("START HIL");
    this->write_register(this->addresses.bases.hil_control, 1);
}

void hil_deployer::stop() {
    spdlog::info("STOP HIL");
    this->write_register(this->addresses.bases.hil_control, 0);
}

hardware_sim_data_t hil_deployer::get_hardware_sim_data(nlohmann::json &specs) {
    hardware_sim_data_t sim_data;
    hw.disable_bus_access();
    deploy(specs);
    start();
    hw.enable_bus_access();
    auto [rom, control] = hw.get_hardware_simulation_data();

    std::string outputs;
    for(auto [key, name]:bus_labels) {
        std::ranges::replace(name, ' ', '_');
        outputs += std::to_string(key) + ":" + name + "\n";
    }

    std::string inputs;
    for(auto &[name, tb]:inputs_labels) {
        auto ep = name;
        std::ranges::replace(ep, ' ', '_');
        inputs += ep + "," + std::to_string(tb.peripheral) + "," + std::to_string(tb.destination) + "," + std::to_string(tb.selector) + "," + std::to_string(tb.core_idx) +"\n";
    }

    sim_data.inputs = inputs;
    sim_data.outputs = outputs;
    sim_data.cores = rom;
    sim_data.control = control;

    return sim_data;
}

std::vector<uint32_t> hil_deployer::calculate_timebase_divider() {
    std::vector<uint32_t>core_dividers;
    auto programs = dispatcher.get_programs();
    for(auto &p: programs){
        if(p.sampling_frequency == 0) {
            core_dividers.push_back(1);
        } else {
            double clock_period = 1/hil_clock_frequency;
            double total_length = (p.program.program_length.fixed_portion + n_channels[p.name]*p.program.program_length.per_channel_portion)*clock_period;

            double max_frequency = 1/total_length;

            while(timebase_frequency/timebase_divider > max_frequency){
                timebase_divider += 1.0;
            }
            core_dividers.push_back((uint32_t) timebase_frequency/p.sampling_frequency);
        }
    }

    return core_dividers;

}

std::vector<uint32_t> hil_deployer::calculate_timebase_shift() {

    struct shift_calc_data {
        std::string name;
        uint32_t execution_order;
        uint32_t program_length;
    };

    auto programs = dispatcher.get_programs();

    std::vector<shift_calc_data> calc_data_v;
    for(auto &p :programs ){
        auto l = p.program.program_length.fixed_portion + n_channels[p.name]*p.program.program_length.per_channel_portion;

        calc_data_v.emplace_back(p.name, execution_order[p.name], l);
    }


    std::ranges::sort(calc_data_v, [](const shift_calc_data &a, const shift_calc_data &b){
        return a.execution_order < b.execution_order;
    });

    // TODO: characterize and use dma bus characteristics to set this dynamically
    uint32_t inter_core_buffer = 90;

    std::unordered_map<std::string, uint32_t>ordered_shifts;
    ordered_shifts[calc_data_v[0].name] = 2;

    uint32_t  next_starting_point = calc_data_v[0].program_length + 2 + inter_core_buffer;

    for(int i = 1; i<calc_data_v.size(); i++){
        ordered_shifts[calc_data_v[i].name] = next_starting_point;
        next_starting_point += calc_data_v[i].program_length + inter_core_buffer;
    }
    std::vector<uint32_t> shifts;

    for(int i = 0; i < calc_data_v.size(); i++) {
        min_timebase += calc_data_v[i].program_length + inter_core_buffer;
    }

    for(const auto &p:programs){
        shifts.push_back(ordered_shifts[p.name]);
    }

    return shifts;
}



