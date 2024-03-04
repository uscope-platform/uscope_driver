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
#ifndef USCOPE_DRIVER_COMMAND_PROCESSOR_HPP
#define USCOPE_DRIVER_COMMAND_PROCESSOR_HPP

#include <sstream>

#include "hw_interface/fpga_bridge.hpp"
#include "response.hpp"
#include "command.hpp"
#include "configuration.hpp"
#include "driver_version.h"

#include "server_frontend/endpoints/control_endpoints.hpp"
#include "server_frontend/endpoints/cores_endpoints.hpp"
#include "server_frontend/endpoints/scope_endpoints.hpp"

class command_processor {
public:
    explicit command_processor(std::shared_ptr<fpga_bridge> &hw, std::shared_ptr<scope_manager> &sc);
    nlohmann::json process_command(std::string command, nlohmann::json &arguments);

private:
    nlohmann::json process_null();
    nlohmann::json process_get_version(nlohmann::json &arguments);

    std::shared_ptr<fpga_bridge> hw;
    control_endpoints control_ep;
    cores_endpoints cores_ep;
    scope_endpoints scope_ep;

    bool logging_enabled;
};


#endif //USCOPE_DRIVER_COMMAND_PROCESSOR_HPP

