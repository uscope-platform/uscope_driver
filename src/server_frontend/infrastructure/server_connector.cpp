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

#include <algorithm>
#include <cstring>


std::atomic_bool server_stop_req;

/// Initialize server connector, bining and listening on the reception socket, and setting up the event loop as necessary
/// \param base event loop base
/// \param port port over which to listen
server_connector::server_connector() {
}

void server_connector::set_interfaces(const std::shared_ptr<bus_accessor> &ba, const std::shared_ptr<scope_accessor> &sa) {
    core_processor.setup_interfaces(ba, sa);
}

void server_connector::start_server() {


    asio::io_context io_context;
    asio::ip::tcp::acceptor a(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::tcp::v4(), runtime_config.server_port));

    while (!server_stop_req){
        asio::ip::tcp::socket s(io_context);
        spdlog::info("The server is ready to accept connections");
        a.accept(s);

        auto  remote_ep = s.remote_endpoint();
        auto remote_ad = remote_ep.address();
        spdlog::trace("Connected to {0}:{1}", remote_ad.to_string(), remote_ep.port());

        try {
            while(true){
                if(auto command_obj = receive_command(s)){

                    std::string error_message;

                    if(!commands::validate_schema(command_obj.value(), commands::command, error_message)){
                        nlohmann::json resp;
                        resp["response_code"] = responses::as_integer(responses::invalid_cmd_schema);
                        resp["data"] = "DRIVER ERROR: Invalid command object received\n"+ error_message;
                        send_response(s, resp);
                        continue;
                    }


                    std::string command = command_obj.value().at("cmd");
                    auto arguments = command_obj.value().at("args");
                    nlohmann::json resp = core_processor.process_command(command, arguments);

                    send_response(s,resp);
                } else {
                    break;
                }
            }
        } catch (const std::exception &e) {
            spdlog::warn("Disconnecting client: {0}", e.what());
        }
        s.close();
    }
}


std::optional<nlohmann::json> server_connector::receive_command(asio::ip::tcp::socket &s) {

    uint32_t message_size;
    do{
        std::array<uint8_t, 4> raw_command_size{};
        std::error_code error;
        asio::read(s, asio::buffer(raw_command_size), error);
        if(error){
            if(error == asio::error::eof) return {};
            throw std::runtime_error(error.message());
        }
        ack_message(s);
        std::memcpy(&message_size, raw_command_size.data(), sizeof(message_size));
    } while(message_size == 0);

    spdlog::trace("waiting reception of {0} bytes", message_size);

    if(message_size > max_msg_size){
        throw std::runtime_error(
                "Incoming message size of " + std::to_string(message_size) +
                " bytes exceeds the maximum allowed size of " + std::to_string(max_msg_size) + " bytes");
    }

    if(message_buffer.size() < message_size){
        message_buffer.resize(message_size);
    }

    size_t transfer_progressive = 0;
    while(transfer_progressive < message_size){
        std::error_code error;
        size_t cur_read = s.read_some(asio::buffer(chunk_buffer), error);
        if(error)  {
            if(error == asio::stream_errc::eof) throw std::system_error(asio::error::make_error_code(asio::stream_errc::eof));
            throw std::runtime_error(error.message());
        }
        if(cur_read == 0) throw std::runtime_error("Connection closed while receiving message payload");

        size_t to_copy = std::min(cur_read, static_cast<size_t>(message_size) - transfer_progressive);
        std::ranges::copy_n(chunk_buffer.begin(), to_copy, message_buffer.begin() + transfer_progressive);
        transfer_progressive += to_copy;
    }

    std::string message(message_buffer.data(), message_size);
    return nlohmann::json::from_msgpack(message);
}

void server_connector::send_response(asio::ip::tcp::socket &s, const nlohmann::json &j) {
    auto raw_response = nlohmann::json::to_msgpack(j);
    uint32_t resp_size = raw_response.size();
    std::array<uint8_t, 4> raw_resp_size{};
    std::memcpy(raw_resp_size.data(), &resp_size, sizeof(resp_size));
    s.send(asio::buffer(raw_resp_size));
    wait_ack(s);
    s.send(asio::buffer(raw_response));
}

void server_connector::ack_message(asio::ip::tcp::socket &s) {
    s.send(asio::buffer("k",1));
}

void server_connector::wait_ack(asio::ip::tcp::socket &s) {
    std::array<uint8_t, 1> raw_command_size{};
    std::error_code error;
    asio::read(s, asio::buffer(raw_command_size), error);
    if(error) {
        if(error == asio::error::eof) throw std::system_error(asio::error::make_error_code(asio::error::eof));
        throw std::runtime_error(error.message());
    }
}





