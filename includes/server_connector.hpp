//
// Created by fils on 20/08/2020.
//

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
    server_connector(int port, const std::string &driver_file, unsigned int dma_buffer_size, bool debug);
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
