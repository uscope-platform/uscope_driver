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
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>..

#ifndef USCOPE_DRIVER_SERVER_HANDLER_HPP
#define USCOPE_DRIVER_SERVER_HANDLER_HPP

#include <cstdint>


#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include "commands.hpp"
#include "command_processor.hpp"
#include "response.hpp"


class server_connector {
public:
    server_connector(int port, const std::string &driver_file, unsigned int dma_buffer_size, bool debug, bool log);
    void start_server();
    void process_connection(int connection_fd);

    static void send_response(response &resp, int connection_fd);
    static command parse_raw_command(const std::string& received_str);
    void stop_server();
    ~server_connector();
    int sockfd;
    command_processor core_processor;
    std::atomic_bool server_stop_req;
    struct sockaddr_in servaddr{};
};


#endif //USCOPE_DRIVER_SERVER_HANDLER_HPP
