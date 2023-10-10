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


    CLI::App app{"Low level Driver for the uScope interface system"};

    bool emulate_hw = false;
    bool log_command = false;
    bool read_version = false;
    std::string scope_data_source;
    int log_level = 0;

    app.add_flag("--debug", emulate_hw, "Emulate hardware for debug on regular processors");
    app.add_flag("--log", log_command, "Log the received commands on the standard output");
    app.add_option("--log_level", log_level, "Log the received commands on the standard output");
    app.add_option("--scope_source", scope_data_source, "Path for the scope data source");
    app.add_flag("--version", read_version, "Print the software version information");

    CLI11_PARSE(app, argc, argv);

    if(read_version){
        std::cout << "The current uscope_driver version is:\n    " + uscope_driver_versions<<std::endl;
        return 0;
    }



    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, intHandler);
    signal(SIGTERM,intHandler);
    signal(SIGKILL,intHandler);

    std::string scope_driver_file  = "/dev/uscope_data";;



    if(!scope_data_source.empty()){
        scope_driver_file = scope_data_source;
    }
    std::jthread ctrl_thread;
    std::jthread cores_thread;
    std::jthread scope_thread;
    if(emulate_hw){
        std::string control_bus_device = "DEVNAME=uscope_BUS_0";
        std::string fCore_bus_device = "DEVNAME=uscope_BUS_1";
        std::string data_device = "DEVNAME=uscope_data";
        ctrl_thread = std::jthread([&](){kernel_emulator(control_bus_device, 300, 0);});
        cores_thread = std::jthread([&](){kernel_emulator(fCore_bus_device, 301, 1);});
        scope_thread = std::jthread([&](){kernel_emulator(data_device, 302, 2);});
        usleep(10000);
    }

    unsigned int channel_buffer_size = 1024;

    std::cout<< "debug mode: "<< std::boolalpha <<emulate_hw<<std::endl;
    std::cout<< "logging: "<< std::boolalpha <<log_command<<std::endl;


    connector = new server_connector(6666, scope_driver_file, channel_buffer_size,
                                     emulate_hw,log_command, log_level);

    connector->start_server();
    return 0;
}
