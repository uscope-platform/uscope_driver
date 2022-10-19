// Copyright 2021 University of Nottingham Ningbo China
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
    std::string scope_data_source;

    app.add_flag("--debug", emulate_hw, "Emulate hardware for debug on regular processors");
    app.add_flag("--log", log_command, "Log the received commands on the standard output");
    app.add_option("--scope_source", scope_data_source, "Path for the scope data source");

    CLI11_PARSE(app, argc, argv);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, intHandler);
    signal(SIGTERM,intHandler);
    signal(SIGKILL,intHandler);

    std::string scope_driver_file  = "/dev/uscope_data";;

    bool emulate_scope = false;

    if(!scope_data_source.empty()){
        scope_driver_file = scope_data_source;
        emulate_scope = true;
    }

    unsigned int scope_buffer_size = 6144;

    std::cout<< "debug mode: "<< std::boolalpha <<emulate_hw<<std::endl;
    std::cout<< "logging: "<< std::boolalpha <<log_command<<std::endl;


    connector = new server_connector(6666, scope_driver_file, scope_buffer_size,
                                     emulate_hw, emulate_scope,log_command);

    connector->start_server();
    return 0;
}
