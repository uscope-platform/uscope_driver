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

#define _FILE_OFFSET_BITS 64

#include "uscope_driver.hpp"
#include "configuration.hpp"


configuration runtime_config;

interfaces_dictionary if_dict;

/// Handler for SIGINT in order to stop the event loop on CTRL+C
/// \param args
void intHandler(int args) {
    pclose(emulator_fd);
    exit(0);
}


/// main is the entry point of the program, as per standard C conventions
/// \param argc standard C arguments count
/// \param argv standard C arguments array
/// \return Program exit value
int main (int argc, char **argv) {


    CLI::App app{"Low level Driver for the uScope interface system"};

    bool emulate_hw = false;
    bool debug_hil = false;
    bool external_emu = false;
    bool log_command = false;
    bool read_version = false;
    std::string scope_data_source;
    int log_level = 0;

    app.add_flag("--external_emulator", external_emu, "Use external kernel emulator");
    app.add_flag("--debug", emulate_hw, "Enable debug features to allow running off target");
    app.add_flag("--debug_hil", debug_hil, "Write intermediate steps for hil deployment debugging");
    app.add_flag("--log", log_command, "Log the received commands on the standard output");
    app.add_option("--log_level", log_level, "Log the received commands on the standard output");
    app.add_option("--scope_source", scope_data_source, "Path for the scope data source");
    app.add_flag("--version", read_version, "Print the software version information");

    CLI11_PARSE(app, argc, argv);

    if(read_version){
        spdlog::info("The current uscope_driver version is:\n {0}", uscope_driver_versions);
        return 0;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, intHandler);
    signal(SIGTERM,intHandler);
    signal(SIGKILL,intHandler);


    if(emulate_hw){
        if_dict.set_arch("emulate");

    } else{
        auto arch = std::getenv("ARCH");
        if_dict.set_arch(arch);
    }


    runtime_config.emulate_hw = emulate_hw;
    runtime_config.server_port = 6666;
    runtime_config.debug_hil = debug_hil;


    spdlog::set_level(spdlog::level::info);

    spdlog::set_pattern("[%l] %v");

    spdlog::info("Debug mode: {0}", emulate_hw);
    spdlog::info("Logging mode: {0}", log_command);


    auto ba = std::make_shared<bus_accessor>(runtime_config.emulate_hw);
    auto sa = std::make_shared<scope_accessor>();


    //std::array<server_connector, 4> workers_pool;

    server_connector connector;

    connector.set_interfaces(ba, sa);
    connector.start_server();


    return 0;
}
