

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

#include "hw_interface/fpga_bridge.hpp"
#include "emulator/emulator_manager.hpp"
#include "fCore_isa.hpp"

#include "deployment/deployer_base.hpp"


template <class hw_bridge>
class hil_deployer : public deployer_base<hw_bridge>{
public:
    hil_deployer();
    void set_hw_bridge(std::shared_ptr<hw_bridge>  &h);

    responses::response_code deploy(fcore::emulator::emulator_specs &specs, const std::vector<fcore::program_bundle> &programs);

    void select_output(uint32_t channel, const output_specs_t& output);
    void set_input(uint32_t address, uint32_t value, std::string core);
    void start();
    void stop();
private:


    void setup_sequencer(uint16_t n_cores, std::vector<uint32_t> divisors, const std::vector<uint32_t>& orders);
    void setup_cores(uint16_t n_cores);

    void check_reciprocal(const std::vector<uint32_t> &program);
    std::vector<uint32_t> calculate_timebase_divider(const std::vector<fcore::program_bundle> &programs,
                                                                   std::vector<uint32_t> n_c);

    std::vector<uint32_t> calculate_timebase_shift(const std::vector<fcore::program_bundle> &programs,
                                                     std::vector<uint32_t> n_c);


    std::map<std::string, uint32_t> cores_idx;
    float hil_clock_frequency = 100e6;



    std::vector<uint32_t> n_channels;

    double timebase_frequency;
    double timebase_divider = 1;

    bool full_cores_override;

};


#endif //USCOPE_DRIVER_HIL_DEPLOYER_HPP
