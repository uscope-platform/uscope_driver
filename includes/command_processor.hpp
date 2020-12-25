// Copyright (C) 2020 Filippo Savi - All Rights Reserved

// This file is part of uscope_driver.

// uscope_driver is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the
// License.

// uscope_driver is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>.
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
    uint32_t process_single_write_register(const std::string& operand_1, const std::string& operand_2);
    uint32_t process_proxied_single_write_register(const std::string& operand_1, const std::string& operand_2);
    uint32_t process_single_read_register(const std::string& operand_1, response &resp);
    uint32_t process_bulk_write_register(const std::string& operand_1, const std::string& operand_2);
    uint32_t process_bulk_read_register(const std::string& operand_1, response &resp);
    uint32_t process_start_capture(const std::string& operand);
    uint32_t process_read_data(response &resp);
    uint32_t process_check_capture_progress(response &resp);
    uint32_t process_apply_program(const std::string& operand_1, const std::string& operand_2);

    fpga_bridge hw;
};


#endif //USCOPE_DRIVER_COMMAND_PROCESSOR_HPP

