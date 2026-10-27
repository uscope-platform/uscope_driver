

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

fcore::result<void> deployer_base::write_register(uint64_t addr, uint32_t val) {
    spdlog::info("write 0x{0:x} to address {1:x}", val, addr);
    nlohmann::json write;
    write["type"] = "direct";
    write["address"] = addr;
    write["value"] = val;

    auto written = hw.single_write_register(write);
    if(!written) return fcore::fail(std::move(written).error());
    return {};
}

fcore::result<void> deployer_base::write_registers(const std::vector<register_write> &writes) {
    for(auto &[addr, val]:writes){
        if(auto written = write_register(addr, val); !written) return written;
    }
    return {};
}

fcore::result<void> deployer_base::load_core(uint64_t address, const std::vector<uint32_t> &program) {
    auto loaded = hw.apply_program(address, program);
    if(!loaded) return fcore::fail(std::move(loaded).error());
    return {};
}

fcore::result<uint16_t> deployer_base::setup_output_dma(uint64_t address, const std::string &core_name, uint32_t n_channels) {
    spdlog::info("SETUP DMA FOR CORE: {0} AT ADDRESS 0x{1:x}", core_name, address);
    spdlog::info("------------------------------------------------------------------");
    int current_io = 0;
    for(auto &i:bus_map){
        if(i.source_id == core_name){
            auto entry = setup_output_entry(i, address, current_io, n_channels);
            if(!entry) return fcore::fail(std::move(entry).error());
            current_io++;
        }
    }
    if(auto written = write_register(address, current_io); !written) return fcore::fail(std::move(written).error());
    spdlog::info("------------------------------------------------------------------");
    return static_cast<uint16_t>(current_io);
}

fcore::result<void> deployer_base::setup_output_entry(const fcore::deployer_interconnect_slot &e, uint64_t dma_address, uint32_t io_progressive, uint32_t n_channels) {

    if(e.source_io_address > 0xfff){
        return fcore::fail(fcore::error_code::invalid_argument, "The maximum source address in an interconnect is 0xFFF");
    }
    if(e.destination_bus_address > 0xfff){
        return fcore::fail(fcore::error_code::invalid_argument, "The maximum destination address in an interconnect is 0xFFF");
    }
    if(e.source_channel > 0xF){
        return fcore::fail(fcore::error_code::invalid_argument, "The maximum number source of channels in an interconnect is 16");
    }
    if(e.destination_channel > 0xF){
        return fcore::fail(fcore::error_code::invalid_argument, "The maximum number destination of channels in an interconnect is 16");
    }


    uint16_t source_portion = (e.source_io_address & 0xFFF) |((e.source_channel & 0xF)<<12);
    uint16_t destination_portion = (e.destination_bus_address & 0xFFF) |((e.destination_channel & 0xF)<<12);

    uint32_t mapping = (destination_portion<<16) | source_portion;

    uint64_t mapping_address = dma_address + +3*4 + io_progressive*4;
    spdlog::info("map core io address: ({0},{1}) to hil bus address: ({2},{3})",
                 e.source_io_address,e.source_channel, e.destination_bus_address, e.destination_channel);
    if(auto written = write_register(mapping_address, mapping); !written) return written;

    auto n_dma_channels = 16;

    uint64_t metadata_address = dma_address + 4*(n_dma_channels + 3) + io_progressive*4;
    std::string signal_name;
    auto bus_dest = (e.destination_bus_address & 0xFFF) |((e.destination_channel & 0xF)<<16);
    if(n_channels == 1) {
        signal_name = e.source_id + "." + e.source_name;
    } else {
        signal_name = e.source_id + "." + e.source_name + '[' + std::to_string(e.source_channel) + ']';
    }
    bus_labels[bus_dest] = signal_name;

    if(ivs.contains(e.source_id)) {
        if(ivs[e.source_id].contains(e.source_name)) {
            auto iv = ivs[e.source_id][e.source_name];
            if(auto written = write_registers({{dma_address+8, io_progressive}, {dma_address+4, iv[e.source_channel]}}); !written) return written;
        }
    }

    if(e.metadata.type == fcore::type_float){
        return write_register(metadata_address, get_metadata_value(
                32,
                true,
                true
        ));
    }
    return write_register(metadata_address, get_metadata_value(
            e.metadata.width,
            e.metadata.is_signed,
            false
    ));
}

uint32_t deployer_base::get_metadata_value(uint8_t size, bool is_signed, bool is_float) {
    uint32_t ret = size-8;
    ret = is_signed ? (ret | 0x10): ret;
    ret = is_float  ?( ret | 0x20 ): ret;

    return ret;
}

fcore::result<void> deployer_base::setup_core(uint64_t core_address, uint32_t n_channels) {
    return write_register(core_address, n_channels);
}

fcore::result<void> deployer_base::setup_memories(uint64_t base_address, std::vector<fcore::memory_init_value> init_values, uint32_t n_channels) {
    spdlog::info("------------------------------------------------------------------");
    for(auto &[address, value]:init_values){

        if(std::holds_alternative<std::vector<float>>(value)){
            auto vect = std::get<std::vector<float>>(value);
            if(vect.size() == n_channels) {
                for(int i = 0; i < n_channels; i++) {
                    auto v = float_to_uint32(vect[i]);
                    auto register_addr = base_address + ((address[0] & 0xFF) + (i<<8))*4;
                    if(auto written = write_register(register_addr, v); !written) return written;
                }
            } else {
                auto v = float_to_uint32(vect[0]);
                auto register_addr = base_address+address[0]*4;
                if(auto written = write_register(register_addr, v); !written) return written;
            }

        } else {
            auto vect = std::get<std::vector<uint32_t>>(value);
            if(vect.size() == n_channels) {
                for(int i = 0; i < n_channels; i++) {
                    auto v = std::get<std::vector<uint32_t>>(value)[i];
                    auto register_addr = base_address + ((address[0] & 0xFF) + (i<<8))*4;
                    if(auto written = write_register(register_addr, v); !written) return written;
                }
            } else {
                auto v = std::get<std::vector<uint32_t>>(value)[0];
                auto register_addr = base_address+address[0]*4;
                if(auto written = write_register(register_addr, v); !written) return written;
            }
        }
    }
    spdlog::info("------------------------------------------------------------------");
    return {};
}

fcore::result<void> deployer_base::setup_input(
    const fcore::deployed_core_inputs &in,
    uint64_t const_ip_address,
    uint32_t const_idx,
    uint32_t target_channel,
    std::string core_name,
    bool is_multichannel
) {

    if(in.source_type ==fcore::constant_input || in.source_type == fcore::time_series_input){
        uint32_t address =in.address[0] + (target_channel<<16);


        input_metadata_t metadata;

        uint32_t input_value;
        metadata.is_float = in.metadata.type == fcore::type_float;
        metadata.core = core_name;
        metadata.name = in.name;
        auto selector = const_idx + (target_channel<<16);
        metadata.const_ip_addr = {const_ip_address, selector};
        metadata.dest = address;
        metadata.channel = target_channel;

        if(std::holds_alternative<std::vector<float>>(in.data[0])){
            auto c_val = std::get<std::vector<float>>(in.data[0])[0];
            input_value = float_to_uint32(c_val);
        } else {
            input_value = std::get<std::vector<uint32_t >>(in.data[0])[0];
        }

        spdlog::info("set default value {0} for input {1} at address {2} on core {3}",input_value, in.name, address, core_name);
        std::string input_path;
        if(is_multichannel) {
            input_path = core_name + "[" + std::to_string(target_channel) + "]" + "." + in.name;
        } else {
            input_path = core_name + "." + in.name;
        }

        inputs_labels[input_path] = {
            metadata.const_ip_addr.first + fcore_constant_engine.const_lsb,
            metadata.dest,
            metadata.const_ip_addr.second,
        };

        if(auto written = write_registers({
            {metadata.const_ip_addr.first + fcore_constant_engine.const_selector, metadata.const_ip_addr.second},
            {metadata.const_ip_addr.first + fcore_constant_engine.const_dest, metadata.dest},
            {metadata.const_ip_addr.first + fcore_constant_engine.const_lsb, input_value}
        }); !written) return written;

        inputs.push_back(metadata);
    } else if(in.source_type == fcore::random_input) {
        uint32_t address =in.address[0] + (target_channel<<16);
        if(auto written = write_register( const_ip_address + (active_random_inputs+1)*4, address); !written) return written;
        active_random_inputs++;

        if(is_multichannel){
            bus_labels[address] = core_name.substr(0, core_name.find_last_of('[')) + "." + in.name +  + "[" + std::to_string(target_channel) + "]";
        } else {
            bus_labels[address] = core_name + "." + in.name;
        }

    } else if(in.source_type == fcore::waveform_input) {
        uint32_t address =in.address[0] + (target_channel<<16);
        if(is_multichannel){
            bus_labels[address] = core_name.substr(0, core_name.find_last_of('[')) + "." + in.name +  + "[" + std::to_string(target_channel) + "]";
        } else {
            bus_labels[address] = core_name + "." + in.name;
        }

        return std::visit([&](auto &&arg) { return setup_waveform(const_ip_address, arg, address); }, in.waveform_parameters);
    }
    return {};
}

fcore::result<void> deployer_base::update_input_value(const std::string &core_name,  const std::string &name, uint16_t channel, double raw_value) {\
    std::string core_identifier = core_name + '[' + std::to_string(channel) + ']';
    for(auto &in:inputs){
        if(in.name == name && in.core == core_name && in.channel == channel){
            uint32_t value =static_cast<uint32_t>(raw_value);
            if(in.is_float) value = float_to_uint32(static_cast<float>(raw_value));
            if(auto written = write_registers({
                {in.const_ip_addr.first + fcore_constant_engine.const_selector, in.const_ip_addr.second},
                {in.const_ip_addr.first + fcore_constant_engine.const_dest, in.dest},
                {in.const_ip_addr.first + fcore_constant_engine.const_lsb, value}
            }); !written) return written;
            spdlog::info("HIL SET INPUT: set value {0} for input at address {1}, on core {2}", raw_value, in.dest, core_identifier);
        }
    }
    return {};
}

fcore::result<void> deployer_base::setup_waveform(const uint64_t address, const fcore::square_wave_parameters &p, uint32_t dest) {
    uint32_t channel = (0xFFF0000 & dest) >>16;
    uint64_t period = p.period[channel]*hil_clock_frequency;
    uint64_t t_on = p.t_on[channel]*hil_clock_frequency;
    uint64_t t_delay = p.t_delay[channel]*hil_clock_frequency;
    if(auto written = write_registers({
        {address + square_wave_gen.channel_selector, active_waveforms},
        {address +square_wave_gen.v_on, float_to_uint32(p.v_on[channel])},
        {address +square_wave_gen.v_off, float_to_uint32(p.v_off[channel])},
        {address +square_wave_gen.t_delay, static_cast<uint32_t>(t_delay)},
        {address +square_wave_gen.t_on, static_cast<uint32_t>(t_on)},
        {address +square_wave_gen.period, static_cast<uint32_t>(period)},
        {address +square_wave_gen.dest_out, dest},
        {address + square_wave_gen.shape_selector, 0},
        {address +square_wave_gen.user_out, get_metadata_value(32,true,true)}
    }); !written) return written;
    active_waveforms++;
    return write_register(address + square_wave_gen.active_channels,  active_waveforms);
}

fcore::result<void> deployer_base::setup_waveform(const uint64_t address, const fcore::sine_wave_parameters &p, uint32_t channel) {
    return fcore::fail(fcore::error_code::not_supported, "Not implemented in hardware yet");
}

fcore::result<void> deployer_base::setup_waveform(const uint64_t address, const fcore::triangle_wave_parameters &p, uint32_t channel) {
    return fcore::fail(fcore::error_code::not_supported, "Not implemented in hardware yet");
}

void deployer_base::set_accessor(const std::shared_ptr<bus_accessor> &ba) {
    hw.set_accessor(ba);
}




