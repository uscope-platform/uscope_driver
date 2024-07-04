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

#include "server_frontend/infrastructure/server_connector.hpp"


std::atomic_bool server_stop_req;

/// Initialize server connector, bining and listening on the reception socket, and setting up the event loop as necessary
/// \param base event loop base
/// \param port port over which to listen
server_connector::server_connector(std::shared_ptr<fpga_bridge> &hw, std::shared_ptr<scope_manager> &sc)
: core_processor(hw, sc) , sock(ctx, zmq::socket_type::rep){

    spdlog::info("Server connector initialization started");
    address = "tcp://*:" + std::to_string(runtime_config.server_port);
    sock.bind(address);
    spdlog::info("listening at address: " + address);

}


void server_connector::start_server() {

    while (!server_stop_req){
        zmq::message_t request;
        //  Wait for next request from client
        const size_t size = 1024;
        zmq::message_t msg(size);
        auto ret = sock.recv(msg);
        auto rec_str = msg.to_string();
        nlohmann::json command_obj = nlohmann::json::parse(msg.to_string());
        std::string error_message;
        if(!commands::validate_schema(command_obj, commands::command, error_message)){
            nlohmann::json resp;
            resp["response_code"] = responses::as_integer(responses::invalid_cmd_schema);
            resp["data"] = "DRIVER ERROR: Invalid command object received\n"+ error_message;
            send_response(resp);
            return;
        }

        std::string command = command_obj["cmd"];
        auto arguments = command_obj["args"];
        nlohmann::json resp = core_processor.process_command(command, arguments);
        send_response(resp);
    }
}



void server_connector::send_response(nlohmann::json &resp) {

    std::vector<std::uint8_t> binary_response = nlohmann::json::to_msgpack(resp);
    auto raw_data = reinterpret_cast<const char*>(binary_response.data());

    zmq::message_t msg(raw_data, binary_response.size());
    sock.send(msg, zmq::send_flags::none);
}

server_connector::~server_connector() {
    sock.unbind(address);
}





