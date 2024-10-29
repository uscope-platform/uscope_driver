

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

#ifndef USCOPE_DRIVER_CUSTOM_DEPLOYER_HPP
#define USCOPE_DRIVER_CUSTOM_DEPLOYER_HPP


#include <nlohmann/json.hpp>
#include <memory>

#include "hw_interface/fpga_bridge.hpp"
#include "deployment/deployer_base.hpp"

class custom_deployer : public deployer_base{
public:
    custom_deployer();
    void set_hw_bridge(std::shared_ptr<fpga_bridge>  &h);

    responses::response_code deploy(fcore::emulator::emulator_specs &specs, const std::vector<fcore::program_bundle> &programs);
private:
    float clock_frequency;
    std::map<std::string, uint32_t> cores_idx;

};


#endif //USCOPE_DRIVER_CUSTOM_DEPLOYER_HPP
