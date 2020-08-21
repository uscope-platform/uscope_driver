//
// Created by fils on 20/08/2020.
//

#ifndef USCOPE_DRIVER_SERVER_HANDLER_HPP
#define USCOPE_DRIVER_SERVER_HANDLER_HPP

#include <cstdint>
#include <cstring>

#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#include "commands.h"
#include "command_processor.hpp"

class server_connector {
public:
    server_connector(int port, const std::string &driver_file, unsigned int dma_buffer_size, bool debug);
    void start_server();
    void process_connection(int connection_fd);

    void send_response(response_t *response, int connection_fd);
    command_t *parse_raw_command(char *received_string);
    void stop_server();
    ~server_connector();
    int sockfd;
    command_processor core_processor;
    std::atomic_bool server_stop_req;
    struct sockaddr_in servaddr{};
};


#endif //USCOPE_DRIVER_SERVER_HANDLER_HPP
