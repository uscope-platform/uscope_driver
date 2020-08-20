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
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>.

#ifndef USCOPE_DRIVER_SERVER_CONNECTOR_H
#define USCOPE_DRIVER_SERVER_CONNECTOR_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <event.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>

#include "commands.h"
#include "command_processor.h"

void init_server_connector(struct event_base * 	base, int port);
void cleanup_server_connector(void);
static void setup_connection(int sockfd, short evtype, void *arg);
int process_connection(int connection_fd);

void send_response(response_t *response, int connection_fd);
command_t *parse_raw_command(char *received_string);

struct event connect_event;
int sockfd;

#endif //USCOPE_DRIVER_SERVER_CONNECTOR_H
