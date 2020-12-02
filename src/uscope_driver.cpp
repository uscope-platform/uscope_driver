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

#include "uscope_driver.hpp"


server_connector *connector;

/// Handler for SIGINT in order to stop the event loop on CTRL+C
/// \param args
void intHandler(int args) {
    connector->stop_server();
    exit(0);
}


/// main is the entry point of the program, as per standard C conventions
/// \param argc standard C arguments count
/// \param argv standard C arguments array
/// \return Program exit value
int main (int argc, char **argv) {

    debug_mode = false;

    CLI::App app{"Low level Driver for the uScope interface system"};

    bool emulate_hw = false;
    bool log_command = false;

    app.add_flag("--debug", emulate_hw, "Emulate hardware for debug on regular processors");
    app.add_flag("--log", log_command, "Log the received commands on the standard output");

    CLI11_PARSE(app, argc, argv);


    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, intHandler);
    signal(SIGTERM,intHandler);
    signal(SIGKILL,intHandler);

    std::string scope_driver_file = "/dev/uCube_dev_0";
    unsigned int scope_buffer_size = 1026;

    std::cout<< "debug mode: "<< std::boolalpha <<emulate_hw<<std::endl;
    std::cout<< "logging: "<< std::boolalpha <<log_command<<std::endl;


    connector = new server_connector(6666, scope_driver_file, scope_buffer_size, emulate_hw,log_command);
    connector->start_server();
    return 0;
}
