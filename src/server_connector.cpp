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

#include "server_connector.hpp"



/// Initialize server connector, bining and listening on the reception socket, and setting up the event loop as necessary
/// \param base event loop base
/// \param port port over which to listen
server_connector::server_connector(int port, const std::string &driver_file, unsigned int dma_buffer_size, bool debug, bool log) :core_processor(driver_file,dma_buffer_size,debug,log) {

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

    command c = parse_raw_command(const_cast<char *>(command_str.c_str()));

    response resp = core_processor.process_command(c);

    send_response(resp, connection_fd);

}

void server_connector::send_response(response &resp, int connection_fd) {

    uint16_t body_present;
    if(!resp.body.empty()) body_present = 1;
    else body_present = 0;

    uint16_t raw_response_header[3] = {resp.opcode, resp.return_code,body_present};
    send(connection_fd, &raw_response_header[0],3*sizeof(uint16_t), 0);
    if(body_present){
        uint64_t response_size = resp.body.size()*sizeof(uint32_t);
        char* raw_response_size = (char *) &response_size;
        write(connection_fd, raw_response_size, sizeof(uint64_t));
        send(connection_fd,&resp.body[0], response_size,0);
    }
}

/// This helper function parses the received commands.
/// Each command is constructed of three space delimited strings: OPCODE OPERAND_1 and OPERAND_2
/// the opcode is a simple integer number that defines the type of command, while the two operands are command dependent
/// data fields.
/// \param received_string C string that contains the raw command
/// \return Structure containing the parsed command content
command server_connector::parse_raw_command(const std::string& received_str) {
    command parsed_command;
    std::istringstream rec_stream(received_str);
    std::string opcode;
    rec_stream >> opcode;
    parsed_command.opcode =  strtol(opcode.c_str(), nullptr, 0);
    rec_stream >> parsed_command.operand_1;
    rec_stream >> parsed_command.operand_2;

    return parsed_command;
}

server_connector::~server_connector() {
    close(sockfd);
}

void server_connector::stop_server() {
    core_processor.stop_scope();
    server_stop_req = true;
}


