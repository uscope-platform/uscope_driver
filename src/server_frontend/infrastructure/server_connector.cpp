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
server_connector::server_connector(){
}

void server_connector::set_interfaces(const std::shared_ptr<bus_accessor> &ba, const std::shared_ptr<scope_accessor> &sa) {
    core_processor.setup_interfaces(ba, sa);
}

void server_connector::start_server() {


    asio::io_context io_context;
    asio::ip::tcp::acceptor a(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::tcp::v4(), runtime_config.server_port));

    while (!server_stop_req){
        asio::ip::tcp::socket s(io_context);
        a.accept(s);
        spdlog::info("The server is ready to accept connections");
        while(true){
            nlohmann::json command_obj;
            try {
                command_obj = receive_command(s);
            } catch(std::system_error &e){
                break;
            }

            std::string error_message;

            if(!commands::validate_schema(command_obj, commands::command, error_message)){
                nlohmann::json resp;
                resp["response_code"] = responses::as_integer(responses::invalid_cmd_schema);
                resp["data"] = "DRIVER ERROR: Invalid command object received\n"+ error_message;
                send_response(s, resp);
                continue;
            }


            std::string command = command_obj["cmd"];
            auto arguments = command_obj["args"];
            nlohmann::json resp = core_processor.process_command(command, arguments);

            send_response(s,resp);
        }

    }
}


nlohmann::json server_connector::receive_command(asio::ip::tcp::socket &s) {
    constexpr uint32_t max_msg_size = 1 << 16;

    std::array<uint8_t, 4> raw_command_size{};
    uint32_t cur_size = 0;
    do{
        std::error_code error;
        cur_size += s.read_some(asio::buffer(raw_command_size, 4), error);
        if(error) {
            if(error == asio::stream_errc::eof) throw std::system_error();
            throw std::runtime_error(error.message());
        }
    }
    while(cur_size <4);

    uint32_t message_size = *reinterpret_cast<uint32_t*>(raw_command_size.data());

    spdlog::info("waiting reception of {0} bytes", message_size);

    char raw_msg[max_msg_size];
    cur_size = 0;
    do{
        std::error_code error;
        cur_size += s.read_some(asio::buffer(raw_msg), error);
        if(error)  {
            if(error == asio::stream_errc::eof) throw std::system_error();
            throw std::runtime_error(error.message());
        }
    }
    while(cur_size <message_size);

    std::string message(raw_msg, message_size);
    return nlohmann::json::from_msgpack(message);
}

void server_connector::send_response(asio::ip::tcp::socket &s, const nlohmann::json &j) {
    auto raw_response = nlohmann::json::to_msgpack(j);
    uint32_t resp_size = raw_response.size();
    auto *raw_resp_size =reinterpret_cast<uint8_t*>(&resp_size);
    s.send(asio::buffer(raw_resp_size, 4));
    s.send(asio::buffer(raw_response));
}





