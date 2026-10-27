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

#ifndef USCOPE_DRIVER_SCOPE_CONTROLLER_HPP
#define USCOPE_DRIVER_SCOPE_CONTROLLER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include "errors/result.hpp"
#include "hw_interface/bus/bus_accessor.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "hw_interface/hw_address_maps.hpp"
#include "hw_interface/scope_scaling.hpp"

/// Executes the scope commands received through the websocket (scaling, acquisition setup, scope address, DMA).
/// The commands touch the hardware and can block, so they are executed one at a time on a thread of their own, in the
/// order they are received, leaving the event loop that streams the data free.
class scope_controller {
public:
    scope_controller(std::shared_ptr<bus_accessor> ba, std::shared_ptr<scope_scaling> scaling);

    /// Executes the command, resuming the caller once done.
    /// \return {"ok": true, "data": ...} or {"ok": false, "error": "..."}, never throws
    boost::asio::awaitable<nlohmann::json> execute(std::string cmd, nlohmann::json args);

private:
    nlohmann::json handle(const std::string &cmd, const nlohmann::json &args);

    fcore::result<void> set_scaling_factors(const nlohmann::json &args);
    fcore::result<void> set_acquisition(const nlohmann::json &args);
    fcore::result<std::string> get_acquisition_status();
    fcore::result<void> set_scope_address(const nlohmann::json &args);
    fcore::result<void> disable_dma(const nlohmann::json &args);

    // only accessed from the command thread
    uint64_t scope_base_address = 0;
    bool first_load = true;
    scope_address_map am;
    fpga_bridge hw;

    std::shared_ptr<scope_scaling> scaling;
    boost::asio::thread_pool pool{1};  // last, so that it is joined before the other members are destroyed
};

#endif //USCOPE_DRIVER_SCOPE_CONTROLLER_HPP
