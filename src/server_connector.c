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



/// This helper function parses the commands that are received through redis pub-sub messages.
/// Each command is constructed of three space delimited strings: OPCODE OPERAND_1 and OPERAND_2
/// the opcode is a simple integer number that defines the type of command, while the two operands are command dependent
/// data fields.
/// \param received_string C string that contains the raw command
/// \return Structure containing the parsed command content
command_t *parse_raw_command(char *received_string){
    command_t *parsed_command;
    parsed_command = malloc(sizeof(command_t));
    char * saveptr;

    parsed_command->opcode = strtol(strtok_r(received_string, " ", &saveptr), NULL, 0);
    parsed_command->operand_1 = strtok_r(NULL, " ", &saveptr);
    parsed_command->operand_2 = strtok_r(NULL, " ", &saveptr);
    return parsed_command;
}



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

int process_connection(int connection_fd){

    uint8_t raw_command_length[8];
    uint64_t command_length = 0;

    // Get length of the command string
    read(connection_fd,&raw_command_length, sizeof(raw_command_length));
    command_length = (uint64_t) *raw_command_length;

    // Get command string
    char *command = (char*) calloc(command_length, sizeof(char)+1);
    read(connection_fd, command, command_length);

    command_t *received_command = parse_raw_command(command);

    response_t *response = malloc(sizeof(response_t));
    response->body = calloc(1024, sizeof(int32_t));

    process_command(received_command, response);
    send_response(response, connection_fd);

    free(received_command);
    free(response->body);
    free(response);

}



void send_response(response_t *response, int connection_fd) {
    char *raw_response = malloc(10000* sizeof(char));
        if(response->body_size!=0){

            sprintf(raw_response, "%u %u\n", response->opcode, response->return_code);
            for(uint32_t i = 0; i<response->body_size; i++){
                char result[100];
                if(i<response->body_size-1) sprintf(result," %u,", response->body[i]);
                else sprintf(result," %u", response->body[i]);
                strcat(raw_response, result);
            }
        } else {
            sprintf(raw_response, "%u %u", response->opcode, response->return_code);
        }

        uint64_t response_size = 10000*sizeof(char);
        char* raw_response_size = (char *) &response_size;

        write(connection_fd, raw_response_size, sizeof(uint64_t));

        write(connection_fd, raw_response, response_size);


}


/// This function cleans up the event loop and sockets used by the server_connector, it must be called once done.
void cleanup_server_connector(void){
    if(event_del(&connect_event)) {
        printf("Error removing connection event from the event loop.\n");
    }
    close(sockfd);

}