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

#include "server_connector.hpp"



/// Initialize server connector, bining and listening on the reception socket, and setting up the event loop as necessary
/// \param base event loop base
/// \param port port over which to listen
server_connector::server_connector(int port, const std::string &driver_file, unsigned int dma_buffer_size, bool emulate_control,bool emulate_scope, bool log) : core_processor(driver_file, dma_buffer_size, emulate_control, emulate_scope,log) {

    if(log){
        std::cout << "server_connector initialization started"<< std::endl;
    }
    logging = log;
    server_stop_req = false;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cerr << "socket creation failed" << std::endl;
        exit(0);
    }


    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if ((bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
        std::cerr << "socket bind failed" << std::endl;
        exit(-1);
    }

    if(listen(sockfd, 2)) {
        std::cerr << "Error listening to listening socket" << std::endl;
        exit(-1);
    }

    if(log){
        std::cout << "server_connector initialization ended"<< std::endl;
    }

}


void server_connector::start_server() {

    int connfd;
    socklen_t addrlen = sizeof(servaddr);

    while (!server_stop_req){
        connfd = accept(sockfd, (struct sockaddr *)&servaddr, &addrlen);
        if(connfd < 0) {
            if(errno != EWOULDBLOCK && errno != EAGAIN) {
                std::cerr <<"Error accepting an incoming connection" <<std::endl;
                exit(-1);
            }
        }

        process_connection(connfd);

        close(connfd);
    }
}


void server_connector::process_connection(int connection_fd) {
    
    uint64_t command_length = 0;
    
    std::string length_str(10, 0);

    ssize_t rec = 0;
    do {
        int result = recv(connection_fd, &length_str[rec], sizeof(length_str) - rec, 0);
        rec += result;
    }
    while (rec < 10);

    command_length = std::stoul(length_str, nullptr, 10);


    std::string dummy_resp = "ok";
    send(connection_fd, &dummy_resp[0], 2, 0);

    // Get command string

    std::string command_str(command_length+1, 0);

    rec = 0;
    do {
        int result = recv(connection_fd, &command_str[rec], 2*sizeof(command_str), 0);
        rec += result;
    }
    while (rec < command_length);

    nlohmann::json command_obj = nlohmann::json::parse(command_str);
    std::string error_message;
    if(!validate_command(command_obj, json_specs::command, error_message)){
        nlohmann::json resp;
        resp["return_code"] = RESP_INVALID_COMMAND_SCHEMA;
        resp["data"] = "DRIVER ERROR: Invalid command object received\n"+ error_message;
        send_response(resp, connection_fd);
        return;
    }

    uint32_t command = command_obj["cmd"];
    auto arguments = command_obj["args"];
    nlohmann::json resp = core_processor.process_command(command, arguments);
    send_response(resp, connection_fd);

}

void server_connector::send_response(nlohmann::json &resp, int connection_fd) {

    std::vector<std::uint8_t> binary_response = nlohmann::json::to_msgpack(resp);

    uint32_t response_length = htonl( binary_response.size());
    send(connection_fd, &response_length, sizeof(uint32_t), MSG_CONFIRM);
    auto raw_data = reinterpret_cast<const char*>(binary_response.data());
    send(connection_fd,raw_data, binary_response.size(),MSG_CONFIRM);
}

server_connector::~server_connector() {
    close(sockfd);
}

void server_connector::stop_server() {
    core_processor.stop_scope();
    server_stop_req = true;
}

bool server_connector::validate_command(nlohmann::json &cmd, nlohmann::json &schema, std::string &error) {


    nlohmann::json_schema::json_validator validator;
    try {
        validator.set_root_schema(schema);
        validator.validate(cmd);
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
    return true;
}



