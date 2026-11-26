//   Copyright 2026 Filippo Savi <filssavi@gmail.com>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef USCOPE_DRIVER_RPU_MANAGER_HPP
#define USCOPE_DRIVER_RPU_MANAGER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "errors/result.hpp"

/// Loads, starts and stops the programs of the RPU (R5) cores through the kernel remoteproc framework.
///
/// Remoteproc reads the firmware file when a core is started, so a program is staged in the firmware
/// directory, the core is started, and the file is deleted straight after. There is no plain start: a
/// stopped core is restarted by loading its program again.
class rpu_manager {
public:
    /// Stops the core if it is not offline, then loads the ELF program and starts the core
    fcore::result<void> load_program(uint8_t core, const std::vector<uint8_t> &program);
    /// Brings the core offline, doing nothing if it already is
    fcore::result<void> stop(uint8_t core);
    /// The remoteproc state of the core: offline, running, crashed, ...
    fcore::result<std::string> get_state(uint8_t core);

private:
    static fcore::result<void> write_file(const std::string &path, const std::string &content);
    static fcore::result<std::string> read_file(const std::string &path);
    static fcore::result<void> start_core(uint8_t core, const std::string &remoteproc);
};

#endif //USCOPE_DRIVER_RPU_MANAGER_HPP
