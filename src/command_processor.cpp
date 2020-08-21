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

command_processor::command_processor(const std::string &driver_file, unsigned int dma_buffer_size, bool debug) : hw(driver_file, dma_buffer_size, debug) {
}

/// This function, core of the driver operation, is called upon message reception and parsing, acts as a dispatcher,
/// sending the command to one of several other functions that will further parse the operands and further call
/// appropriate functions in the fpga_bridge.c or scope_handler.c files. Process_command also populates
/// the response structure that defines what will be sent back to the server.
/// \param Command Command received from the uscope_server
response command_processor::process_command(command_t *command) {
    response resp;
    resp.opcode = command->opcode;
    switch(command->opcode){
        case C_NULL_COMMAND:
            break;
        case C_LOAD_BITSTREAM:
            resp.return_code = process_load_bitstream(command->operand_1);
            break;
        case C_SINGLE_REGISTER_READ:
            resp.return_code = process_single_read_register(command->operand_1, resp);
            break;
        case C_SINGLE_REGISTER_WRITE:
            resp.return_code = process_single_write_register(command->operand_1, command->operand_2);
            break;
        case C_BULK_REGISTER_READ:
            resp.return_code =  process_bulk_read_register(command->operand_1, resp);
            break;
        case C_BULK_REGISTER_WRITE:
            resp.return_code = process_bulk_write_register(command->operand_1, command->operand_2);
            break;
        case C_START_CAPTURE:
            resp.return_code = process_start_capture(command->operand_1);
            break;
        case C_PROXIED_WRITE:
            resp.return_code = process_proxied_single_write_register(command->operand_1, command->operand_2);
            break;
        case C_READ_DATA:
            resp.return_code = process_read_data(resp);
            break;
        case C_CHECK_CAPTURE_PROGRESS:
            resp.return_code = process_check_capture_progress(resp);
            break;
    }
    return resp;
}


///
/// \param Operand bitstream name
/// \return
uint32_t command_processor::process_load_bitstream(char *operand) {
    return hw.load_bitstream(operand);
}

///
/// \param operand_1 Register address
/// \param operand_2 Value to write
/// \return Success
uint32_t command_processor::process_single_write_register(char *operand_1, char *operand_2) {
    uint32_t address = strtoul(operand_1,NULL, 0);
    uint32_t value = strtoul(operand_2,NULL, 0);
    return hw.single_write_register(address,value);
}

///
/// \param operand_1 Comma delimited list of address pairs to write to (comprised of address of the proxy and address to write)
/// \param operand_2 Value to write
/// \return Success
uint32_t command_processor::process_proxied_single_write_register(char *operand_1, char *operand_2) {
    char *addr_sptr;
    char* proxy_addr_s = strtok_r(operand_1, ",", &addr_sptr);
    char* reg_addr_s = strtok_r(NULL, ",", &addr_sptr);
    uint32_t proxy_addr = strtoul(proxy_addr_s,NULL, 0);
    uint32_t reg_addr = strtoul(reg_addr_s,NULL, 0);
    uint32_t value = strtoul(operand_2,NULL, 0);
    return hw.single_proxied_write_register(proxy_addr, reg_addr, value);
}

///
/// \param operand_1 Register address
/// \param response Response object where the read result will be placed
/// \return Success
uint32_t command_processor::process_single_read_register(char *operand_1, response &resp) {
    uint32_t address = strtoul(operand_1,nullptr, 0);
    return hw.single_read_register(address, resp.body);
}

///
/// \param operand_1 Comma delimited list of addresses to write to
/// \param operand_2 Comma delimited list of values to write
/// \return Success
uint32_t command_processor::process_bulk_write_register(char *operand_1, char *operand_2) {
    uint32_t write_addresses[100];
    uint32_t write_data[100];
    uint32_t array_pointer = 0;

    char *addr_sptr, *val_sptr;
    char* addr_tok = strtok_r(operand_1, ",", &addr_sptr);
    char* val_tok = strtok_r(operand_2, ",", &val_sptr);

    while (addr_tok != NULL){
        write_addresses[array_pointer] = strtoul(addr_tok, NULL, 0);
        write_data[array_pointer] = strtoul(val_tok, NULL, 0);
        array_pointer++;

        addr_tok = strtok_r(NULL, ",", &addr_sptr);
        val_tok = strtok_r(NULL, ",", &val_sptr);
    }

    return hw.bulk_write_register(write_addresses, write_data, array_pointer+1);
}

///
/// \param operand_1 Comma delimited list of addresses to read from
/// \param response Pointer to the response structure where the results of the read will be put
/// \return Success
uint32_t command_processor::process_bulk_read_register(char *operand_1, response &resp) {
    uint32_t read_addresses[100];
    uint32_t array_pointer = 0;

    char *addr_sptr;
    char* addr_tok = strtok_r(operand_1, ",", &addr_sptr);

    while (addr_tok != NULL){
        read_addresses[array_pointer] = strtoul(addr_tok, NULL, 0);
        array_pointer++;
        addr_tok = strtok_r(NULL, ",", &addr_sptr);
    }
    return hw.bulk_read_register(read_addresses, resp.body, array_pointer+1);
}

///
/// \param Operand number of buffers to captire
/// \return Success
uint32_t command_processor::process_start_capture(char *operand) {
    uint32_t n_buffers = strtoul(operand,NULL, 0);
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

void command_processor::stop_scope() {
    hw.stop_scope();
}
