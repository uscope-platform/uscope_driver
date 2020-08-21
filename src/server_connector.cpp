//
// Created by fils on 20/08/2020.
//

#include "server_connector.hpp"



/// Initialize server connector, bining and listening on the reception socket, and setting up the event loop as necessary
/// \param base event loop base
/// \param port port over which to listen
server_connector::server_connector(int port, const std::string &driver_file, unsigned int dma_buffer_size, bool debug) :core_processor(driver_file,dma_buffer_size,debug) {

    server_stop_req = false;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cerr << "socket creation failed" << std::endl;
        exit(0);
    }

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

}


void server_connector::start_server() {

    int connfd;
    socklen_t addrlen = sizeof(servaddr);

    while (!server_stop_req){
        connfd = accept(sockfd, (struct sockaddr *)&servaddr, &addrlen);
        if(connfd < 0) {
            if(errno != EWOULDBLOCK && errno != EAGAIN) {
                printf("Error accepting an incoming connection");
                exit(-1);
            }
        }

        process_connection(connfd);

        close(connfd);
    }
}

void server_connector::process_connection(int connection_fd) {
    uint8_t raw_command_length[8];
    uint64_t command_length = 0;

    // Get length of the command string
    read(connection_fd,&raw_command_length, sizeof(raw_command_length));
    command_length = (uint64_t) *raw_command_length;

    // Get command string
    char *command = (char*) calloc(command_length, sizeof(char)+1);
    read(connection_fd, command, command_length);

    command_t *received_command = parse_raw_command(command);

    response_t *response = (response_t*) malloc(sizeof(response_t));
    response->body = (uint32_t*) calloc(1024, sizeof(int32_t));

    core_processor.process_command(received_command, response);
    send_response(response, connection_fd);

    free(received_command);
    free(response->body);
    free(response);

}

void server_connector::send_response(response_t *response, int connection_fd) {
    char *raw_response = (char*) malloc(10000* sizeof(char));
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

/// This helper function parses the received commands.
/// Each command is constructed of three space delimited strings: OPCODE OPERAND_1 and OPERAND_2
/// the opcode is a simple integer number that defines the type of command, while the two operands are command dependent
/// data fields.
/// \param received_string C string that contains the raw command
/// \return Structure containing the parsed command content
command_t *server_connector::parse_raw_command(char *received_string) {
    command_t *parsed_command;
    parsed_command = (command_t *) malloc(sizeof(command_t));
    char * saveptr;

    parsed_command->opcode = strtol(strtok_r(received_string, " ", &saveptr), NULL, 0);
    parsed_command->operand_1 = strtok_r(NULL, " ", &saveptr);
    parsed_command->operand_2 = strtok_r(NULL, " ", &saveptr);
    return parsed_command;
}

server_connector::~server_connector() {
    close(sockfd);
}

void server_connector::stop_server() {
    core_processor.stop_scope();
    server_stop_req = true;
}

