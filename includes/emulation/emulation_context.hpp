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

#ifndef USCOPE_DRIVER_EMULATION_CONTEXT_HPP
#define USCOPE_DRIVER_EMULATION_CONTEXT_HPP

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "emulator/live_emulation.hpp"

/// What the HIL commands and the scope data are for: the hardware, or the emulator that runs inside the driver
enum class hil_target_kind {
    hardware,
    emulator
};

/// The emulator, and whether the user selected it. The commands and the scope data are served by different threads
/// that have to use the same emulation, so, like the configuration, there is one of them for the whole driver.
/// The target is the hardware until the user selects the emulator.
class emulation_context {
public:
    static emulation_context &instance();

    hil_target_kind get_target() const { return target; }
    bool emulator_selected() const { return target == hil_target_kind::emulator; }
    /// Selecting the hardware stops the emulation: it does not go on running in the background
    void set_target(hil_target_kind new_target);

    std::shared_ptr<fcore::live_emulation> get_emulation() const { return emulation; }

    /// Executes the command if it is one of the emulator: the selection of the target is always for it, the HIL
    /// commands only while it is the target. They take the same arguments and give the same responses as the ones of the
    /// hardware, so that the clients do not tell them apart.
    /// \return the response, or nothing if the command is for the hardware
    std::optional<nlohmann::json> process_command(const std::string &command, const nlohmann::json &arguments);

private:
    emulation_context();

    nlohmann::json process_set_target(const nlohmann::json &arguments);
    nlohmann::json process_get_target();
    nlohmann::json process_deploy(const nlohmann::json &arguments);
    nlohmann::json process_select_out(const nlohmann::json &arguments);
    nlohmann::json process_set_in(const nlohmann::json &arguments);
    nlohmann::json process_start();
    nlohmann::json process_stop();
    nlohmann::json process_get_sampling_frequency();

    std::atomic<hil_target_kind> target = hil_target_kind::hardware;
    std::shared_ptr<fcore::live_emulation> emulation;
};

#endif //USCOPE_DRIVER_EMULATION_CONTEXT_HPP
