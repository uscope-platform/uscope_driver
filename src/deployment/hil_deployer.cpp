

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

template <class hw_bridge>
hil_deployer<hw_bridge>::hil_deployer(std::shared_ptr<hw_bridge> &h) : deployer_base<hw_bridge>(h) {

    full_cores_override = true; // ONLY FULL CORES ARE USED RIGHT NOW

    auto clock_f = std::getenv("HIL_CLOCK_FREQ");

    if(clock_f != nullptr){
        hil_clock_frequency = std::stof(clock_f);
    }
    spdlog::info("HIL CLOCK FREQUENCY: {0}", hil_clock_frequency);
}

template <class hw_bridge>
responses::response_code hil_deployer<hw_bridge>::deploy(fcore::emulator::emulator_specs &specs, const std::vector<fcore::program_bundle> &programs) {


    this->setup_base(specs);

    if(specs.cores.size()>32){
        auto msg = "The HIL SYSTEM ONLY SUPPORTS UP TO 32 CORES AT ONCE";
        spdlog::critical(msg);
        throw std::runtime_error(msg);
    }

    std::vector<uint32_t> frequencies;

    for(auto & c : specs.cores){
        frequencies.push_back(c.sampling_frequency);
    }

    timebase_frequency = std::accumulate(frequencies.begin(), frequencies.end(), 1,[](uint32_t a, uint32_t b){
        return std::lcm(a,b);
    });

    spdlog::info("------------------------------------------------------------------");
    for(int i = 0; i<programs.size(); i++){
        auto core_address = this->addresses.bases.cores_rom + i * this->addresses.offsets.cores_rom;
        spdlog::info("SETUP PROGRAM FOR CORE: {0} AT ADDRESS: 0x{1:x}", specs.cores[i].id, core_address);
        cores_idx[specs.cores[i].id] = i;
        this->load_core(core_address, programs[i].program.binary);
        check_reciprocal(programs[i].program.binary);
    }

    auto dividers = calculate_timebase_divider(programs, n_channels);
    auto shifts = calculate_timebase_shift(programs, n_channels);

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
        auto control_address = this->addresses.bases.cores_control + i * this->addresses.offsets.cores_control;
        this->setup_memories(control_address, programs[i].memories);
    }


    for(auto &c: specs.cores){
        uint64_t complex_base_addr = this->addresses.bases.cores_control + this->addresses.offsets.cores_control*cores_idx[c.id];
        this->setup_inputs(
                c,
                complex_base_addr,
                this->addresses.bases.cores_inputs,
                this->addresses.offsets.cores_inputs
        );
    }

    setup_sequencer(programs.size(), dividers, shifts);
    setup_cores(programs.size());

    return responses::ok;
}

template <class hw_bridge>
void hil_deployer<hw_bridge>::setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& shifts) {
    spdlog::info("SETUP SEQUENCER");
    spdlog::info("------------------------------------------------------------------");

    auto timebase_reg_val = (uint32_t)(hil_clock_frequency/timebase_frequency);


    std::bitset<32> enable;
    for(int i = 0; i<n_cores; i++){
        enable[i] = true;
        this->write_register(this->addresses.bases.controller + this->addresses.offsets.controller + 0x4 + 4 * i, divisors[i]-1);
        this->write_register(this->addresses.bases.controller + this->addresses.offsets.hil_tb + 8 + 4*i, shifts[i]);
    }

    this->write_register(this->addresses.bases.controller + this->addresses.offsets.hil_tb + 4, timebase_reg_val);

    this->write_register(this->addresses.bases.controller + this->addresses.offsets.controller, enable.to_ulong());


    spdlog::info("------------------------------------------------------------------");
}

template <class hw_bridge>
void hil_deployer<hw_bridge>::setup_cores(uint16_t n_cores) {
    spdlog::info("SETUP CORES");
    spdlog::info("------------------------------------------------------------------");
    for(int i = 0; i<n_cores; i++){
        this->setup_core(this->addresses.bases.cores_control + i * this->addresses.offsets.cores_control,  n_channels[i]);
    }
}


template <class hw_bridge>
void hil_deployer<hw_bridge>::check_reciprocal(const std::vector<uint32_t> &program) {
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

template <class hw_bridge>
void hil_deployer<hw_bridge>::select_output(uint32_t channel, const output_specs_t& output) {
    spdlog::info("HIL SELECT OUTPUT: selected output {0}({1},{2}) from core {3} for scope channel {4}",
                 output.source_output,
                 output.address,
                 output.channel,
                 output.core_name,
                 channel);

    auto data = this->get_bus_address(output);
    auto selector = data.first | (data.second <<16);
    this->write_register(this->addresses.bases.scope_mux + 4*channel+ 4, selector);
}

template <class hw_bridge>
void hil_deployer<hw_bridge>::set_input(uint32_t address, uint32_t value, std::string core) {
    this->update_input_value(address, value, core);
}

template <class hw_bridge>
void hil_deployer<hw_bridge>::start() {
    spdlog::info("START HIL");
    this->write_register(this->addresses.bases.hil_control, 1);
}

template <class hw_bridge>
void hil_deployer<hw_bridge>::stop() {
    spdlog::info("STOP HIL");
    this->write_register(this->addresses.bases.hil_control, 0);
}

template <class hw_bridge>
std::vector<uint32_t> hil_deployer<hw_bridge>::calculate_timebase_divider(const std::vector<fcore::program_bundle> &programs, std::vector<uint32_t> n_c) {
    std::vector<uint32_t>core_dividers;

    for(int i = 0; i<programs.size(); i++){
        const auto& p = programs[i];
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

template <class hw_bridge>
std::vector<uint32_t> hil_deployer<hw_bridge>::calculate_timebase_shift(const std::vector<fcore::program_bundle> &programs, std::vector<uint32_t> n_c) {

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

template class hil_deployer<fpga_bridge>;