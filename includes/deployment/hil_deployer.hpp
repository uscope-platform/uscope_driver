

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

#ifndef USCOPE_DRIVER_HIL_DEPLOYER_HPP
#define USCOPE_DRIVER_HIL_DEPLOYER_HPP

#include <spdlog/spdlog.h>
#include <bitset>

#include "options_repository.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "emulator/emulator_dispatcher.hpp"
#include "fCore_isa.hpp"

#include "deployment/deployer_base.hpp"

struct hardware_sim_data_t {
    std::string cores;
    std::string control;
    std::string outputs;
    std::string inputs;
};

class hil_deployer : public deployer_base{
public:
    hil_deployer();
    void set_options_repository(std::shared_ptr<options_repository> &rep) {options = rep;};
    void set_accessor(const std::shared_ptr<bus_accessor> &ba);

    responses::response_code deploy(nlohmann::json &arguments);

    void select_output(uint32_t channel, const output_specs_t& output);
    void set_input(uint32_t address, uint32_t value, std::string core);
    void start();
    void stop();

    hardware_sim_data_t get_hardware_sim_data(nlohmann::json &specs);
private:


    void setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& orders);
    void setup_cores(std::vector<fcore::deployed_program> &programs);

    uint32_t check_reciprocal(const std::vector<uint32_t> &program);
    std::vector<uint32_t> calculate_timebase_divider();

    std::vector<uint32_t> calculate_timebase_shift();


    std::map<std::string, uint32_t> cores_idx;
    float hil_clock_frequency = 100e6;
    uint32_t min_timebase = 0;
    std::unordered_map<std::string, uint32_t> n_channels;
    std::unordered_map<std::string, uint32_t> execution_order;

    double timebase_frequency;
    double timebase_divider = 1;

    std::shared_ptr<options_repository> options;

    bool full_cores_override;
};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
