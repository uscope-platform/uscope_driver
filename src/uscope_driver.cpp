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

#include "uscope_driver.h"


/// Handler for SIGINT in order to stop the event loop on CTRL+C
/// \param args
void intHandler(int args) {
    event_base_loopbreak(main_loop);
    exit(0);
}



/// main is the entry point of the program, as per standard C conventions
/// \param argc standard C arguments count
/// \param argv standard C arguments array
/// \return Program exit value
int main (int argc, char **argv) {
    debug_mode = false;

    if(argc==2){
        if(strcmp(argv[1],"--debug") == 0) debug_mode=true;
    }
    std::string scope_driver_file = "/dev/uio0";
    unsigned int scope_buffer_size = 1024*4;

    command_processor_n processor(fpga_bridge(scope_driver_file, scope_buffer_size, debug_mode));


    signal(SIGPIPE, SIG_IGN);
    main_loop = event_base_new();
    signal(SIGINT, intHandler);

    server_connector connector(main_loop, 6666);

    event_base_dispatch(main_loop);

    return 0;
}