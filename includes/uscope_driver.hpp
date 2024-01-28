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

#ifndef USCOPE_DIR_USCOPE_DRIVER_H
#define USCOPE_DIR_USCOPE_DRIVER_H


#include <CLI/CLI.hpp>


#include <csignal>
#include <cstdint>
#include <regex.h>
#include <argp.h>
#include <thread>

#include <unistd.h>
#include <sys/mman.h>

#include "server_frontend/server_connector.hpp"
#include "hw_interface/interfaces_dictionary.hpp"
#include "version.h"



FILE *emulator_fd;

#endif //USCOPE_DIR_USCOPE_DRIVER_H
