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

#ifndef USCOPE_DIR_USCOPE_DRIVER_H
#define USCOPE_DIR_USCOPE_DRIVER_H





#include <csignal>
#include <cstdint>
#include <regex.h>
#include <argp.h>

#include <unistd.h>
#include <sys/mman.h>


#include "commands.hpp"


#include "command_processor.hpp"

#include "fpga_bridge.hpp"
#include "server_connector.hpp"

#ifdef __cplusplus
extern "C" {
#endif

    int setup_main_loop(void);

    struct event_base *main_loop;

    bool debug_mode;

    struct event *scope_data;

#ifdef __cplusplus
}
#endif

#endif //USCOPE_DIR_USCOPE_DRIVER_H
