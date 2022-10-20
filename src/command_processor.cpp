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
#include "command_processor.hpp"

command_processor::command_processor(const std::string &driver_file, unsigned int dma_buffer_size, bool emulate_control, bool emulate_scope, bool log) : hw(driver_file, dma_buffer_size, emulate_control, emulate_scope, log) {
    logging_enabled = log;
}

/// This function, core of the driver operation, is called upon message reception and parsing, acts as a dispatcher,
/// sending the command to one of several other functions that will further parse the operands and further call
/// appropriate functions in the fpga_bridge.c or scope_handler.c files. Process_command also populates
/// the response structure that defines what will be sent back to the server.
/// \param Command Command received from the uscope_server
nlohmann::json command_processor::process_command(uint32_t command, nlohmann::json &arguments) {

    nlohmann::json response_obj;
    response_obj["cmd"] = command;

#ifdef VERBOSE_LOGGING
    if(logging_enabled){
        std::cout << "Received command: " << command_map[command] << std::endl;
    }
#endif

    switch(command){
        case C_NULL_COMMAND:
            response_obj["body"] = process_null();
            break;
        case C_LOAD_BITSTREAM:
            response_obj["body"] = process_load_bitstream(arguments);
            break;
        case C_SINGLE_REGISTER_READ:
            response_obj["body"] = process_single_read_register(arguments);
            break;
        case C_SINGLE_REGISTER_WRITE:
            response_obj["body"] = process_single_write_register(arguments);
            break;
        case C_START_CAPTURE:
            response_obj["body"] = process_start_capture(arguments);
            break;
        case C_PROXIED_WRITE:
            response_obj["body"] = process_proxied_single_write_register(arguments);
            break;
        case C_READ_DATA:
            response_obj["body"] = process_read_data();
            break;
        case C_CHECK_CAPTURE_PROGRESS:
            response_obj["body"] = process_check_capture_progress();
            break;
        case C_APPLY_PROGRAM:
            response_obj["body"] = process_apply_program(arguments);
            break;
        case C_SET_CHANNEL_WIDTHS:
            response_obj["body"] = process_set_widths(arguments);
            break;
        case C_SET_SCALING_FACTORS:
            response_obj["body"] = process_set_scaling_factors(arguments);
            break;
    }
    return response_obj;
}


///
/// \param Operand bitstream name
/// \return
nlohmann::json command_processor::process_load_bitstream(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string bitstram_name = arguments;
    resp["response_code"] = hw.load_bitstream(bitstram_name);
    return resp;
}

///
/// \param operand_1 Register address
/// \param operand_2 Value to write
/// \return Success
nlohmann::json command_processor::process_single_write_register(nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = hw.single_write_register(arguments);
    return resp;
}

///
/// \param operand_1 Comma delimited list of address pairs to write to (comprised of address of the proxy and address to write)
/// \param operand_2 Value to write
/// \return Success
nlohmann::json command_processor::process_proxied_single_write_register(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string proxy_addr_s = arguments[0];
    std::string reg_addr_s =  arguments[1];
    std::string value_s = arguments[2];

    uint32_t proxy_addr = std::stoul(proxy_addr_s, nullptr, 0);
    uint32_t reg_addr = std::stoul(reg_addr_s, nullptr , 0);
    uint32_t value = std::stoul(value_s, nullptr , 0);
    resp["response_code"] = hw.single_proxied_write_register(proxy_addr, reg_addr, value);
    return resp;
}

///
/// \param operand_1 Register address
/// \param response Response object where the read result will be placed
/// \return Success
nlohmann::json command_processor::process_single_read_register(nlohmann::json &arguments) {
    std::string reg_addr_s = arguments;
    uint32_t address = std::stoul(reg_addr_s, nullptr, 0);
    return hw.single_read_register(address);
}

///
/// \param Operand number of buffers to captire
/// \return Success
nlohmann::json command_processor::process_start_capture(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string n_buffers_s = arguments[0];
    uint32_t n_buffers = std::stoul(n_buffers_s, nullptr, 0);
    resp["response_code"] = hw.start_capture(n_buffers);
    return resp;
}

///
/// \param response Pointer to the structure where the data will eventually be put
/// \return Either success of failure depending on if the data is actually ready
nlohmann::json command_processor::process_read_data() {
    nlohmann::json resp;
    std::vector<float> flt_data;
    resp["response_code"] = hw.read_data(flt_data);
    resp["data"] = flt_data;
    return resp;
}

///
/// \param Response pointer to the response_t structure where the result of the check will be put
/// \return Success
nlohmann::json command_processor::process_check_capture_progress() {
    nlohmann::json resp;
    unsigned int progress;
    resp["response_code"] = hw.check_capture_progress(progress);
    resp["data"] = progress;
    return resp;
}

///
/// \param operand_1 address of the fCore Instance to program
/// \param operand_2 comma delimited list of instructions in the program
/// \return Success
nlohmann::json command_processor::process_apply_program(nlohmann::json &arguments) {
    nlohmann::json resp;
    uint32_t address = arguments["address"];
    std::vector<uint32_t> program = arguments["program"];
    resp["response_code"] = hw.apply_program(address, program);
    return resp;
}


///
/// \param operand_2 comma delimited list of channel widths
/// \return Success
nlohmann::json command_processor::process_set_widths(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::vector<uint32_t> widths = arguments;
    resp["response_code"] = hw.set_channel_widths(widths);
    return resp;
}

nlohmann::json  command_processor::process_set_scaling_factors(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::vector<float> sfs = arguments;
    resp["response_code"] = hw.set_scaling_factors(sfs);
    return resp;
}


void command_processor::stop_scope() {
    hw.stop_scope();
}

nlohmann::json command_processor::process_null() {
    nlohmann::json resp;
    resp["response_code"] = RESP_OK;
    return resp;
}
