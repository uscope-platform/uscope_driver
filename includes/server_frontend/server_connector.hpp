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
// limitations under the License..

#ifndef USCOPE_DRIVER_SERVER_HANDLER_HPP
#define USCOPE_DRIVER_SERVER_HANDLER_HPP

#include <cstdint>


#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include <nlohmann/json.hpp>

#include "command_processor.hpp"
#include "response.hpp"


class server_connector {
public:
    server_connector(int port, const std::string &driver_file, unsigned int dma_buffer_size, bool emulate_control, bool log, int log_level);
    void start_server();
    void stop_server();
    ~server_connector();
private:
    void send_response(nlohmann::json &resp, int connection_fd);
    void process_connection(int connection_fd);
    int sockfd;
    command_processor core_processor;
    bool logging;
    struct sockaddr_in servaddr{};
};


#endif //USCOPE_DRIVER_SERVER_HANDLER_HPP

