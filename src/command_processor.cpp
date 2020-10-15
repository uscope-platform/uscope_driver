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
#include "command_processor.hpp"

command_processor::command_processor(const std::string &driver_file, unsigned int dma_buffer_size, bool debug, bool log) : hw(driver_file, dma_buffer_size, debug, log) {
}

/// This function, core of the driver operation, is called upon message reception and parsing, acts as a dispatcher,
/// sending the command to one of several other functions that will further parse the operands and further call
/// appropriate functions in the fpga_bridge.c or scope_handler.c files. Process_command also populates
/// the response structure that defines what will be sent back to the server.
/// \param Command Command received from the uscope_server
response command_processor::process_command(const command& c) {
    response resp;
    resp.opcode = c.opcode;
    switch(c.opcode){
        case C_NULL_COMMAND:
            break;
        case C_LOAD_BITSTREAM:
            resp.return_code = process_load_bitstream(c.operand_1);
            break;
        case C_SINGLE_REGISTER_READ:
            resp.return_code = process_single_read_register(c.operand_1, resp);
            break;
        case C_SINGLE_REGISTER_WRITE:
            resp.return_code = process_single_write_register(c.operand_1, c.operand_2);
            break;
        case C_BULK_REGISTER_READ:
            resp.return_code =  process_bulk_read_register(c.operand_1, resp);
            break;
        case C_BULK_REGISTER_WRITE:
            resp.return_code = process_bulk_write_register(c.operand_1, c.operand_2);
            break;
        case C_START_CAPTURE:
            resp.return_code = process_start_capture(c.operand_1);
            break;
        case C_PROXIED_WRITE:
            resp.return_code = process_proxied_single_write_register(c.operand_1, c.operand_2);
            break;
        case C_READ_DATA:
            resp.return_code = process_read_data(resp);
            break;
        case C_CHECK_CAPTURE_PROGRESS:
            resp.return_code = process_check_capture_progress(resp);
            break;
        case C_SET_CHANNEL_STATUS:
            resp.return_code = process_set_channel_status(c.operand_1);
            break;
        case C_APPLY_PROGRAM:
            resp.return_code = process_apply_program(c.operand_1, c.operand_2);
            break;
    }
    return resp;
}


///
/// \param Operand bitstream name
/// \return
uint32_t command_processor::process_load_bitstream(const std::string& bitstream_name) {
    return hw.load_bitstream(bitstream_name);
}

///
/// \param operand_1 Register address
/// \param operand_2 Value to write
/// \return Success
uint32_t command_processor::process_single_write_register(const std::string& operand_1, const std::string& operand_2) {
    uint32_t address = std::stoul(operand_1, nullptr, 0);
    uint32_t value = std::stoul(operand_2, nullptr, 0);
    return hw.single_write_register(address,value);
}

///
/// \param operand_1 Comma delimited list of address pairs to write to (comprised of address of the proxy and address to write)
/// \param operand_2 Value to write
/// \return Success
uint32_t command_processor::process_proxied_single_write_register(const std::string& operand_1, const std::string& operand_2) {

    std::string proxy_addr_s = operand_1.substr(0, operand_1.find(','));
    std::string reg_addr_s =  operand_1.substr(operand_1.find(',')+1, operand_1.length());


    uint32_t proxy_addr = std::stoul(proxy_addr_s, nullptr, 0);
    uint32_t reg_addr = std::stoul(reg_addr_s,nullptr , 0);
    uint32_t value = std::stoul(operand_2,nullptr , 0);
    return hw.single_proxied_write_register(proxy_addr, reg_addr, value);
}

///
/// \param operand_1 Register address
/// \param response Response object where the read result will be placed
/// \return Success
uint32_t command_processor::process_single_read_register(const std::string& operand_1, response &resp) {
    uint32_t address = std::stoul(operand_1, nullptr, 0);
    return hw.single_read_register(address, resp.body);
}

///
/// \param operand_1 Comma delimited list of addresses to write to
/// \param operand_2 Comma delimited list of values to write
/// \return Success
uint32_t command_processor::process_bulk_write_register(const std::string& operand_1, const std::string& operand_2) {
    std::vector<uint32_t> write_addresses, write_data;

    std::istringstream iss(operand_1);
    std::string token;
    while (std::getline(iss, token, ','))
    {
        write_addresses.push_back(std::stoul(token, nullptr, 0));
    }

    iss.str(operand_2);
    while (std::getline(iss, token, ','))
    {
        write_data.push_back(std::stoul(token, nullptr, 0));
    }

    return hw.bulk_write_register(write_addresses, write_data);
}

///
/// \param operand_1 Comma delimited list of addresses to read from
/// \param response Pointer to the response structure where the results of the read will be put
/// \return Success
uint32_t command_processor::process_bulk_read_register(const std::string& operand_1, response &resp) {
    std::vector<uint32_t> read_adresses;

    std::istringstream iss(operand_1);
    std::string token;
    while (std::getline(iss, token, ','))
    {
        read_adresses.push_back(std::stoul(token, nullptr, 0));
    }
    return hw.bulk_read_register(read_adresses, resp.body);
}

///
/// \param Operand number of buffers to captire
/// \return Success
uint32_t command_processor::process_start_capture(const std::string& operand) {
    uint32_t n_buffers = std::stoul(operand, nullptr, 0);
    return hw.start_capture(n_buffers);
}

///
/// \param response Pointer to the structure where the data will eventually be put
/// \return Either success of failure depending on if the data is actually ready
uint32_t command_processor::process_read_data(response &resp) {
    return  hw.read_data(resp.body);
}

///
/// \param Response pointer to the response_t structure where the result of the check will be put
/// \return Success
uint32_t command_processor::process_check_capture_progress(response &resp) {
    resp.body.push_back(hw.check_capture_progress());
    return RESP_OK;
}

///
/// \param operand Comma delimited list of channel, status  pairs
/// \return success
uint32_t command_processor::process_set_channel_status(const std::string &operand) {
    int ch_idx = 0;
    std::istringstream iss(operand);
    std::string token;
    std::vector<bool> status_vector;
    while (std::getline(iss, token, ',')) {
        uint32_t channel_status = std::stoul(token, nullptr, 10);
        status_vector.push_back(channel_status==1);
    }
    hw.set_channel_status(status_vector);
    return 0;
}

///
/// \param operand_1 address of the fCore Instance to program
/// \param operand_2 comma delimited list of instructions in the program
/// \return Success
uint32_t command_processor::process_apply_program(const std::string &operand_1, const std::string &operand_2) {

    uint32_t address = std::stoul(operand_1,nullptr , 0);

    std::istringstream iss(operand_2);
    std::string token;
    std::vector<uint32_t> program;
    while (std::getline(iss, token, ',')) {
        uint32_t instruction = std::stoul(token, nullptr, 0);
        program.push_back(instruction);
    }

    return hw.apply_program(address, program);
}


void command_processor::stop_scope() {
    hw.stop_scope();
}
