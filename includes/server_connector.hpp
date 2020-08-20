//
// Created by fils on 20/08/2020.
//

#ifndef USCOPE_DRIVER_SERVER_HANDLER_HPP
#define USCOPE_DRIVER_SERVER_HANDLER_HPP

#include <cstdint>
#include <cstring>

#include <iostream>

#include <sys/socket.h>
#include <event.h>
#include <fcntl.h>
#include <unistd.h>

#include "commands.h"

class server_connector {
public:
    server_connector(struct event_base * 	base, int port);
    void cleanup_server_connector(void);
    static void setup_connection(int sockfd, short evtype, void *arg);
    int process_connection(int connection_fd);

    void send_response(response_t *response, int connection_fd);
    command_t *parse_raw_command(char *received_string);

    ~server_connector();
    struct event connect_event;
    int sockfd;

};


#endif //USCOPE_DRIVER_SERVER_HANDLER_HPP
