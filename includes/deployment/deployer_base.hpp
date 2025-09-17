

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

#ifndef USCOPE_DRIVER_DEPLOYER_BASE_HPP
#define USCOPE_DRIVER_DEPLOYER_BASE_HPP

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "hw_interface/fpga_bridge.hpp"
#include "deployment/hil_bus_map.hpp"
#include "deployment/deployment_utilities.hpp"

struct logic_layout{

    struct logic_layout_bases{

        uint64_t cores_rom;
        uint64_t cores_control;
        uint64_t cores_inputs;
        uint64_t noise_generator;
        uint64_t waveform_generator;
        uint64_t controller;

        uint64_t scope_mux;
        uint64_t hil_control;

    };
    struct logic_layout_offsets{

        uint64_t dma;
        uint64_t hil_tb;
        uint64_t controller;
        uint64_t cores_rom;
        uint64_t cores_control;
        uint64_t cores_inputs;

    };

    void parse_layout_object(const nlohmann::json &obj){
        bases.cores_rom = obj["bases"]["cores_rom"];
        bases.cores_control = obj["bases"]["cores_control"];
        bases.cores_inputs = obj["bases"]["cores_inputs"];
        bases.controller = obj["bases"]["controller"];
        bases.scope_mux = obj["bases"]["scope_mux"];
        bases.hil_control = obj["bases"]["hil_control"];
        bases.waveform_generator = obj["bases"]["waveform_generator"];
        bases.noise_generator = obj["bases"]["noise_generator"];

        offsets.cores_rom = obj["offsets"]["cores_rom"];
        offsets.dma = obj["offsets"]["dma"];
        offsets.cores_control = obj["offsets"]["cores_control"];
        offsets.controller = obj["offsets"]["controller"];
        offsets.cores_inputs = obj["offsets"]["cores_inputs"];
        offsets.hil_tb = obj["offsets"]["hil_tb"];
    };

    nlohmann::json dump_layout_object(){
        nlohmann::json ret;
        ret["offsets"] = nlohmann::json();
        ret["offsets"]["cores_rom"] = offsets.cores_rom;
        ret["offsets"]["cores_control"] = offsets.cores_control;
        ret["offsets"]["controller"] = offsets.controller;
        ret["offsets"]["cores_inputs"] = offsets.cores_inputs;
        ret["offsets"]["dma"] = offsets.dma;
        ret["offsets"]["hil_tb"] = offsets.hil_tb;

        ret["bases"] = nlohmann::json();
        ret["bases"]["cores_rom"] = bases.cores_rom;
        ret["bases"]["cores_control"] = bases.cores_control;
        ret["bases"]["cores_inputs"] = bases.cores_inputs;
        ret["bases"]["controller"] = bases.controller;
        ret["bases"]["scope_mux"] = bases.scope_mux;
        ret["bases"]["hil_control"] = bases.hil_control;
        ret["bases"]["waveform_generator"] = bases.waveform_generator;
        ret["bases"]["noise_generator"] = bases.noise_generator;
        return ret;
    }

    std::string dump() const{
        std::string ret;
        ret += "CORES ROM: BASE " + std::to_string(bases.cores_rom) +
               ", OFFSET " + std::to_string(offsets.cores_rom) + "\n";
        ret += "CORES CONTROL: BASE " + std::to_string(bases.cores_control) +
               ", OFFSET " + std::to_string(offsets.cores_control) + "\n";
        ret += "CORES INPUTS: BASE " + std::to_string(bases.cores_inputs) +
               ", OFFSET " + std::to_string(offsets.cores_inputs) + "\n";
        ret += "DMA OFFSET: " + std::to_string(offsets.dma) + "\n";
        ret += "CONTROLLER: BASE " + std::to_string(bases.controller) +
               ", OFFSET " + std::to_string(offsets.controller) + "\n";

        ret += "HIL CONTROL: BASE " + std::to_string(bases.hil_control) + "\n";
        ret += "SCOPE MUX: BASE " + std::to_string(bases.scope_mux) + "\n";
        ret += "HIL TB: OFFSET " + std::to_string(offsets.hil_tb) + "\n";
        ret += "NOISE GENERATOR: BASE " + std::to_string(bases.noise_generator) + "\n";
        ret += "WAVEFORM GENERATOR: BASE " + std::to_string(bases.waveform_generator) + "\n";
        return ret;
    }
    logic_layout_bases bases;
    logic_layout_offsets offsets;
};


struct input_metadata_t{
    std::string core;
    std::string name;
    std::pair<uint64_t, uint32_t> const_ip_addr;
    uint32_t channel;
    uint32_t dest;
    bool is_float;
};

struct tb_input_addresses_t {
    uint64_t peripheral;
    uint32_t destination;
    uint32_t selector;
    uint32_t core_idx;
};

class deployer_base {
public:
    deployer_base() = default;

    virtual void set_accessor(const std::shared_ptr<bus_accessor> &ba);

    void set_layout_map(const nlohmann::json &obj){
        spdlog::info("SETUP HIL ADDRESS MAP");
        auto dbg = obj.dump(4);
        addresses.parse_layout_object(obj);
        spdlog::trace(addresses.dump());
    };

    nlohmann::json get_layout_map(){
        return addresses.dump_layout_object();
    };

protected:
    bus_address get_bus_address(const output_specs_t& spec){return bus_map.translate_output(spec);}

    void write_register(uint64_t addr, uint32_t val);
    void load_core(uint64_t address, const std::vector<uint32_t> &program);
    void setup_core(uint64_t core_address, uint32_t n_channels);
    void setup_memories(uint64_t address, std::vector<fcore::memory_init_value> init_values,  uint32_t n_channels);

    void setup_input(const fcore::deployed_core_inputs &c, uint64_t const_ip_address, uint32_t const_idx, uint32_t target_channel,std::string core_name);

    uint16_t setup_output_dma(uint64_t address, const std::string& core_name, uint32_t n_channels);
    void setup_output_entry(const fcore::deployer_interconnect_slot &e, uint64_t dma_address, uint32_t io_progressive, uint32_t n_channels);

    static uint32_t get_metadata_value(uint8_t size, bool is_signed, bool is_float);

    void update_input_value(const std::string &core,  const std::string &name, uint32_t value);

    void setup_waveform(uint64_t address, const fcore::square_wave_parameters &p, uint32_t channel);
    void setup_waveform(uint64_t address, const fcore::sine_wave_parameters &p, uint32_t channel);
    void setup_waveform(uint64_t address, const fcore::triangle_wave_parameters &p, uint32_t channel);


    fpga_bridge hw;
    logic_layout addresses;
protected:
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<uint32_t>>> ivs;
    uint32_t active_random_inputs = 0;
    uint32_t active_waveforms = 0;
    double timebase_frequency;
    fcore::emulator_dispatcher dispatcher;
    hil_bus_map bus_map;
    std::map<uint32_t, std::string> bus_labels;
    std::map<std::string, tb_input_addresses_t> inputs_labels;
private:
    std::vector<input_metadata_t> inputs;
};


static constexpr struct {
    uint8_t const_lsb = 0;
    uint8_t const_hsb = 0x4;
    uint8_t const_dest = 0x8;
    uint8_t const_selector = 0xC;
    uint8_t clear_constant = 0x10;
    uint8_t active_channels = 0x14;
    uint8_t const_user = 0x18;
} fcore_constant_engine;


static constexpr struct {
    uint16_t active_channels = 0x0;
    uint16_t shape_selector = 0x4;
    uint16_t channel_selector = 0x8;
    uint16_t v_on = 0xc;
    uint16_t v_off = 0x10;
    uint16_t t_delay = 0x14;
    uint16_t t_on = 0x18;
    uint16_t period = 0x1c;
    uint16_t dest_out = 0x20;
    uint16_t user_out = 0x24;
} square_wave_gen;

#endif //USCOPE_DRIVER_DEPLOYER_BASE_HPP
