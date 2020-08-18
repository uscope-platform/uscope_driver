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

#include "server_connector.h"


/// Sets a file descriptor for non blocking IO
/// \param fd file descriptor to set
/// \return 0 on success -1 otherwise
static int set_nonblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if(flags == -1) {
        printf("Error getting flags on fd %d", fd);
        return -1;
    }
    flags |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags)) {
        printf("Error setting non-blocking I/O on fd %d", fd);
        return -1;
    }

    return 0;
}

/// Initialize server connector, bining and listening on the reception socket, and setting up the event loop as necessary
/// \param base event loop base
/// \param port port over which to listen
void init_server_connector(struct event_base * 	base, int port){

    struct sockaddr_in servaddr;


    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed\n");
        exit(0);
    }

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if ((bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed\n");
        exit(-1);
    }

    if(listen(sockfd, 2)) {
        printf("Error listening to listening socket\n");
        exit(-1);
    }

    if(set_nonblock(sockfd)){
        printf("Error setting listening socket to non-blocking I/O.\n");
        exit(-1);
    }

    event_set(&connect_event, sockfd, EV_READ | EV_PERSIST, setup_connection, base);
    event_base_set(base, &connect_event);
    if(event_add(&connect_event, NULL)) {
        printf("Error scheduling connection event on the event loop.\n");
    }

}

///
/// \param fd
/// \param evtype
/// \param arg
static void setup_connection(int fd, short evtype, void *arg) {
    struct sockaddr_in remote_addr;
    int connfd;
    socklen_t addrlen = sizeof(remote_addr);

    connfd = accept(fd, (struct sockaddr *)&remote_addr, &addrlen);
    if(sockfd < 0) {
        if(errno != EWOULDBLOCK && errno != EAGAIN) {
            printf("Error accepting an incoming connection");
        }
        exit(-1);
    }

    process_connection(connfd);

    close(connfd);
}

void process_connection(int connection_fd){
    int data[1024];
    uint8_t raw_command_length[8];
    uint64_t command_length = 0;

    // Get length of the command string
    read(connection_fd,&raw_command_length, sizeof(raw_command_length));
    for(int i=7; i>=0; i--)
        command_length |= ( (long long)raw_command_length[i] << (i*8u) );

    // Get command string
    char *command = (char*) calloc(command_length, sizeof(char)+1);
    read(connection_fd, command, command_length);
    printf("%s\n", command);
    for (int i = 0; i < 1024; ++i)
    {
        data[i] = rand() % 1000;
    }
    write(connection_fd, data,sizeof(data));
}

/// This function cleans up the event loop and sockets used by the server_connector, it must be called once done.
void cleanup_server_connector(void){
    if(event_del(&connect_event)) {
        printf("Error removing connection event from the event loop.\n");
    }
    close(sockfd);

}