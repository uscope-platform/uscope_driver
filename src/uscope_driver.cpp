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
#include "server_frontend/data_plane/scope_server.hpp"


configuration runtime_config;

interfaces_dictionary if_dict;

/// Handler for SIGINT/SIGTERM, only requests the server to stop so that main can return and tear everything down
/// \param args
void intHandler(int args) {
    server_stop_req = true;
}


/// main is the entry point of the program, as per standard C conventions
/// \param argc standard C arguments count
/// \param argv standard C arguments array
/// \return Program exit value
int main (int argc, char **argv) {


    CLI::App app{"Low level Driver for the uScope interface system"};

    bool debug_hil = false;
    bool log_command = false;
    bool read_version = false;
    int scope_data_port = 8990;
    int log_level = 0;

    app.add_flag("--debug_hil", debug_hil, "Write intermediate steps for hil deployment debugging");
    app.add_option("--scope-port", scope_data_port, "Port for the scope data websocket server")->check(CLI::PositiveNumber);
    app.add_flag("--log", log_command, "Log the received commands on the standard output");
    app.add_option("--log_level", log_level, "Log the received commands on the standard output");
    app.add_flag("--version", read_version, "Print the software version information");

    CLI11_PARSE(app, argc, argv);

    if(read_version){
        spdlog::info("The current uscope_driver version is:\n {0}", uscope_driver_versions);
        return 0;
    }

    signal(SIGPIPE, SIG_IGN);

    // no SA_RESTART: the blocking accept/read in the server must be interrupted by the signal
    struct sigaction stop_action{};
    stop_action.sa_handler = intHandler;
    sigemptyset(&stop_action.sa_mask);
    sigaction(SIGINT, &stop_action, nullptr);
    sigaction(SIGTERM, &stop_action, nullptr);


    if (const char * arch = std::getenv("ARCH")) {
        if(auto selected = if_dict.set_arch(arch); !selected) {
            spdlog::error("ARCH environment variable: {0}", selected.error().what());
            exit(-1);
        }
    } else {
        spdlog::error("The architecture must be selected with the ARCH environment variable");
        exit(-1);
    }

    runtime_config.server_port = 6666;
    runtime_config.debug_hil = debug_hil;

    if(log_command) {
        if(log_level >0) {
            spdlog::set_level(spdlog::level::trace);
        } else {
            spdlog::set_level(spdlog::level::info);
        }
    } else {
        spdlog::set_level(spdlog::level::warn);
    }

    spdlog::set_pattern("[%l] %v");

    spdlog::info("Logging mode: {0}", log_command);
    spdlog::info("Log level: {0}", log_level);


    auto bus = bus_accessor::create();
    if(!bus) {
        spdlog::error("{0}", bus.error().what());
        exit(1);
    }
    auto scope = scope_accessor::create();
    if(!scope) {
        spdlog::error("{0}", scope.error().what());
        exit(1);
    }
    auto ba = bus.value();
    auto sa = scope.value();


    //std::array<server_connector, 4> workers_pool;
    auto tickets = std::make_shared<scope_tickets>();
    scope_server scope_data_source(scope_data_port, ba, sa, tickets);
    std::jthread scope_source_thread([&scope_data_source](std::stop_token stop){ scope_data_source.run(stop); });

    server_connector connector;

    connector.set_interfaces(ba, tickets);
    connector.start_server();

    if (emulator_fd != nullptr) {
        pclose(emulator_fd);
    }


    return 0;
}
