// Copyright 2021 University of Nottingham Ningbo China
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef USCOPE_DRIVER_COMMAND_PROCESSOR_HPP
#define USCOPE_DRIVER_COMMAND_PROCESSOR_HPP

#include <sstream>

#include "fpga_bridge.hpp"
#include "response.hpp"

class command_processor {
public:
    explicit command_processor(const std::string &driver_file, unsigned int dma_buffer_size, bool emulate_control, bool log);
    void stop_scope();
    nlohmann::json process_command(commands::command_code command, nlohmann::json &arguments);

private:
    nlohmann::json process_null();
    nlohmann::json process_load_bitstream(nlohmann::json &arguments);
    nlohmann::json process_set_frequency(nlohmann::json &arguments);
    nlohmann::json process_single_write_register( nlohmann::json &arguments);
    nlohmann::json process_single_read_register(nlohmann::json &arguments);
    nlohmann::json process_start_capture(nlohmann::json &arguments);
    nlohmann::json process_read_data();
    nlohmann::json process_check_capture_progress();
    nlohmann::json process_apply_program(nlohmann::json &arguments);
    nlohmann::json process_set_widths(nlohmann::json &arguments);
    nlohmann::json process_set_scaling_factors(nlohmann::json &arguments);

    fpga_bridge hw;
    bool logging_enabled;
};


#endif //USCOPE_DRIVER_COMMAND_PROCESSOR_HPP

