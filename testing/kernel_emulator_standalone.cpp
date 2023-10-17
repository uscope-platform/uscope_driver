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

#include <csignal>
#include <thread>

#include <unistd.h>
#include <sys/ioctl.h>

#include "hw_emulation/kernel_emulator.hpp"


/// main is the entry point of the program, as per standard C conventions
/// \param argc standard C arguments count
/// \param argv standard C arguments array
/// \return Program exit value
int main (int argc, char **argv) {


    bool emulate_hw = false;
    bool log_command = false;
    std::string scope_data_source;



    signal(SIGPIPE, SIG_IGN);


    std::jthread ctrl_thread;
    std::jthread cores_thread;
    std::jthread scope_thread;

    std::string control_bus_device = "DEVNAME=uscope_BUS_0";
    std::string fCore_bus_device = "DEVNAME=uscope_BUS_1";
    std::string data_device = "DEVNAME=uscope_data";
    ctrl_thread = std::jthread([&](std::stop_token inner_tok){kernel_emulator(control_bus_device, 300, 0);});
    cores_thread = std::jthread([&](std::stop_token inner_tok){kernel_emulator(fCore_bus_device, 301, 1);});
    scope_thread = std::jthread([&](std::stop_token inner_tok){kernel_emulator(data_device, 302, 2);});
    usleep(10000);

    std::cout<< "debug mode: "<< std::boolalpha <<emulate_hw<<std::endl;
    std::cout<< "logging: "<< std::boolalpha <<log_command<<std::endl;

    do {
        usleep(100000);
    } while (std::cin.get() != '\n');


    auto bus = open("/dev/uscope_BUS_0", O_RDWR| O_SYNC);
    ioctl(bus,4);
    bus= open("/dev/uscope_BUS_1", O_RDWR| O_SYNC);
    ioctl(bus,4);
    bus = open("/dev/uscope_data", O_RDWR| O_SYNC);
    ioctl(bus,4);

    return 0;
}
