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
#include "server_frontend/command_processor.hpp"

command_processor::command_processor(const std::string &driver_file, bool emulate_control, bool log, int log_level)
: hw(driver_file, emulate_control, log, log_level) {
    logging_enabled = log;
}

/// This function, core of the driver operation, is called upon message reception and parsing, acts as a dispatcher,
/// sending the command to one of several other functions that will further parse the operands and further call
/// appropriate functions in the fpga_bridge.c or scope_handler.c files. Process_command also populates
/// the response structure that defines what will be sent back to the server.
/// \param Command Command received from the uscope_server
nlohmann::json command_processor::process_command(commands::command_code command, nlohmann::json &arguments) {

    nlohmann::json response_obj;
    response_obj["cmd"] = commands::as_integer(command);

#ifdef VERBOSE_LOGGING
    if(logging_enabled){
        std::cout << "Received command: " << commands::commands_name_map[command] << std::endl;
    }
#endif

    switch(command){
        case commands::null:
            response_obj["body"] = process_null();
            break;
        case commands::load_bitstream:
            response_obj["body"] = process_load_bitstream(arguments);
            break;
        case commands::set_frequency:
            response_obj["body"] = process_set_frequency(arguments);
            break;
        case commands::register_read:
            response_obj["body"] = process_single_read_register(arguments);
            break;
        case commands::register_write:
            response_obj["body"] = process_single_write_register(arguments);
            break;
        case commands::start_capture:
            response_obj["body"] = process_start_capture(arguments);
            break;
        case commands::read_data:
            response_obj["body"] = process_read_data();
            break;
        case commands::check_capture:
            response_obj["body"] = process_check_capture_progress();
            break;
        case commands::apply_program:
            response_obj["body"] = process_apply_program(arguments);
            break;
        case commands::set_channel_widths:
            response_obj["body"] = process_set_widths(arguments);
            break;
        case commands::set_scaling_factors:
            response_obj["body"] = process_set_scaling_factors(arguments);
            break;
        case commands::set_channel_status:
            response_obj["body"] = process_set_channel_status(arguments);
            break;
        case commands::apply_filter:
            response_obj["body"] = process_apply_filter(arguments);
            break;
        case commands::set_channel_signs:
            response_obj["body"] = process_set_channel_signs(arguments);
            break;
        case commands::get_version:
            response_obj["body"] = process_get_version(arguments);
            break;
        case commands::set_scope_data:
            response_obj["body"] = process_set_scope_data(arguments);
    }
    return response_obj;
}


///
/// \param Operand bitstream name
/// \return
nlohmann::json command_processor::process_load_bitstream(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() !=nlohmann::detail::value_t::string) {
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The load bitstream command must be a string";
        return resp;
    }
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
    std::string error_message;
    if(!commands::validate_schema(arguments, commands::write_register, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the write register command\n"+ error_message;
        return resp;
    }
    resp["response_code"] = hw.single_write_register(arguments);
    return resp;
}

///
/// \param operand_1 Register address
/// \param response Response object where the read result will be placed
/// \return Success
nlohmann::json command_processor::process_single_read_register(nlohmann::json &arguments) {
    if(!arguments.is_number()){
        nlohmann::json resp;
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR:The argument for the single read register call must be a numeric value\n";
        return resp;
    }
    uint64_t address = arguments;
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
    std::vector<nlohmann::json> resp_data;
    resp["response_code"] = hw.read_data(resp_data);
    resp["data"] = resp_data;
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
    std::string error_message;
    if(!commands::validate_schema(arguments, commands::load_program, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the apply program command\n"+ error_message;
        return resp;
    }
    uint64_t address = arguments["address"];
    std::vector<uint32_t> program = arguments["program"];
    resp["response_code"] = hw.apply_program(address, program);
    return resp;
}


///
/// \param operand_2 comma delimited list of channel widths
/// \return Success
nlohmann::json command_processor::process_set_widths(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set channel widths command must be an array\n";
        return resp;
    }
    if(arguments.empty()){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set channel widths can not be an empty array\n";
        return resp;
    }
    if(!std::all_of(arguments.begin(), arguments.end(), [](const nlohmann::json& el){ return el.is_number(); })){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set channel widths command must be numeric values\n";
        return resp;
    }
    std::vector<uint32_t> widths = arguments;
    resp["response_code"] = hw.set_channel_widths(widths);
    return resp;
}

nlohmann::json  command_processor::process_set_scaling_factors(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set scaling factor command must be an array\n";
        return resp;
    }
    if(arguments.empty()){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set scaling factor command can not be an empty array\n";
        return resp;
    }
    if(!std::all_of(arguments.begin(), arguments.end(), [](const nlohmann::json& el){ return el.is_number(); })){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set scaling factor command must be numeric values\n";
        return resp;
    }
    std::vector<float> sfs = arguments;
    resp["response_code"] = hw.set_scaling_factors(sfs);
    return resp;
}


void command_processor::stop_scope() {
    hw.stop_scope();
}

nlohmann::json command_processor::process_null() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    return resp;
}

nlohmann::json command_processor::process_set_frequency(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set clock frequency command must be an array\n";
        return resp;
    }
    std::vector<uint32_t> freq = arguments;
    resp["response_code"] = hw.set_clock_frequency(freq);
    return resp;
}

nlohmann::json command_processor::process_set_channel_status(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::object){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set channel status command must be an array of objects\n";
        return resp;
    }
    std::unordered_map<int, bool> status;
    for(auto &item:arguments.items()){
        status[std::stoi(item.key())] = item.value();
    }
    resp["response_code"] = hw.set_channel_status(status);
    return resp;
}

nlohmann::json command_processor::process_set_channel_signs(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set channel signs command must be an array of objects\n";
        return resp;
    }
    std::unordered_map<int, bool> signs;
    for(auto &item:arguments.items()){
        signs[std::stoi(item.key())] = item.value();
    }
    resp["response_code"] = hw.set_channel_signed(signs);
    return resp;
}

nlohmann::json command_processor::process_apply_filter(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string error_message;
    if(!commands::validate_schema(arguments, commands::apply_filter_schema, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the apply filter command\n"+ error_message;
        return resp;
    }
    uint32_t address = arguments["address"];
    std::vector<uint32_t> taps = arguments["taps"];
    resp["response_code"] = hw.apply_filter(address, taps);
    return resp;
}

nlohmann::json command_processor::process_get_version(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::string){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the get version command must be a string\n";
        return resp;
    }
    std::string component = arguments;
    if(component == "driver"){
        resp["data"] = uscope_driver_versions;
    } else if(component == "module"){
        resp["data"] = hw.get_module_version();
    } else if(component == "hardware"){
        resp["data"] = hw.get_hardware_version();
    } else {
        resp["data"] = "DRIVER ERROR: Unknown component\n";
    }
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

nlohmann::json command_processor::process_set_scope_data(nlohmann::json &arguments) {
    nlohmann::json resp;
    commands::scope_data sd;
    sd.enable_address = arguments["enable"];
    sd.buffer_address = arguments["buffer_address"];

    resp["response_code"] = hw.set_scope_data(sd);
    return resp;
}



