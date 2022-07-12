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
#include "command.hpp"

class command_processor {
public:
    explicit command_processor(const std::string &driver_file, unsigned int dma_buffer_size, bool debug, bool log);
    void stop_scope();
    response process_command(const command& c);

private:
    uint32_t process_load_bitstream(const std::string& bitstream_name);
    uint32_t process_single_write_register(const std::string& operand_1);
    uint32_t process_proxied_single_write_register(const std::string& operand_1, const std::string& operand_2);
    uint32_t process_single_read_register(const std::string& operand_1, response &resp);
    uint32_t process_bulk_read_register(const std::string& operand_1, response &resp);
    uint32_t process_start_capture(const std::string& operand);
    uint32_t process_read_data(response &resp);
    uint32_t process_check_capture_progress(response &resp);
    uint32_t process_apply_program(const std::string& operand_1, const std::string& operand_2);
    uint32_t process_set_widths(const std::string &operand_1);

    fpga_bridge hw;
};


#endif //USCOPE_DRIVER_COMMAND_PROCESSOR_HPP

