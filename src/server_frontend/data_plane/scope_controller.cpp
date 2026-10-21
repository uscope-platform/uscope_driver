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

#include "server_frontend/data_plane/scope_controller.hpp"
#include "deployment/deployment_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unistd.h>
#include <spdlog/spdlog.h>

namespace asio = boost::asio;

namespace {
    /// A command with wrong arguments, the message is sent back to the client
    struct invalid_command : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /// Reads an unsigned integer argument, tolerating the integral floating point values produced by javascript
    uint64_t unsigned_argument(const nlohmann::json &args, const char *name) {
        if (!args.is_object() || !args.contains(name)) throw invalid_command(std::string("missing argument ") + name);
        const auto &v = args[name];
        if (v.is_number_unsigned()) return v;
        if (v.is_number_integer() && v.get<int64_t>() >= 0) return v.get<int64_t>();
        if (v.is_number_float()) {
            double d = v;
            if (d >= 0 && d == std::floor(d)) return static_cast<uint64_t>(d);
        }
        throw invalid_command(std::string("argument ") + name + " must be an unsigned integer");
    }

    const nlohmann::json &required(const nlohmann::json &args, const char *name) {
        if (!args.is_object() || !args.contains(name)) throw invalid_command(std::string("missing argument ") + name);
        return args[name];
    }
}

scope_controller::scope_controller(std::shared_ptr<bus_accessor> ba, std::shared_ptr<scope_scaling> scaling)
    : scaling(std::move(scaling)) {
    hw.set_accessor(ba);
}

asio::awaitable<nlohmann::json> scope_controller::execute(std::string cmd, nlohmann::json args) {
    // the function object is kept alive by co_spawn until the coroutine is done
    co_return co_await asio::co_spawn(
        pool.get_executor(),
        [this, cmd = std::move(cmd), args = std::move(args)]() -> asio::awaitable<nlohmann::json> {
            co_return handle(cmd, args);
        },
        asio::use_awaitable);
}

nlohmann::json scope_controller::handle(const std::string &cmd, const nlohmann::json &args) {
    nlohmann::json reply;
    reply["ok"] = true;
    try {
        if (cmd == "set_scaling_factors") {
            set_scaling_factors(args);
        } else if (cmd == "set_acquisition") {
            set_acquisition(args);
        } else if (cmd == "get_acquisition_status") {
            reply["data"] = get_acquisition_status();
        } else if (cmd == "set_scope_address") {
            set_scope_address(args);
        } else if (cmd == "disable_scope_dma") {
            disable_dma(args);
        } else {
            throw invalid_command("unknown scope command " + cmd);
        }
    } catch (const std::exception &e) {
        spdlog::warn("Scope command {0} failed: {1}", cmd, e.what());
        reply = {{"ok", false}, {"error", std::string("DRIVER ERROR: ") + e.what()}};
    }
    return reply;
}

void scope_controller::set_scaling_factors(const nlohmann::json &args) {
    if (!args.is_array() || args.empty()) {
        throw invalid_command("the scaling factors must be a non empty array");
    }
    if (!std::all_of(args.begin(), args.end(), [](const nlohmann::json &el){ return el.is_number(); })) {
        throw invalid_command("the scaling factors must be numeric values");
    }
    std::vector<float> factors = args;
    spdlog::info("SET_SCALING_FACTORS: {0}", args.dump());
    scaling->set(factors);
}

void scope_controller::set_acquisition(const nlohmann::json &args) {
    const auto trigger = required(args, "trigger").get<std::string>();
    const float level = required(args, "level");
    const uint64_t source = unsigned_argument(args, "source");
    const auto mode = required(args, "mode").get<std::string>();
    const uint64_t trigger_point = unsigned_argument(args, "trigger_point");
    const uint64_t prescaler = unsigned_argument(args, "prescaler");
    required(args, "level_type");

    if (scope_base_address == 0) return;
    if (source == 0) throw invalid_command("the trigger source is numbered from 1");

    spdlog::info("SET ACQUISITION: {0} mode with {1} trigger on channel {2} at level {3} for scope at address {4:x}",
                 mode, trigger, source, level, scope_base_address);
    const uint64_t scope_internal_addr = scope_base_address + am.internal_base;

    uint32_t trg_mode = 0;
    if (trigger == "rising_edge") {
        trg_mode = 0;
    } else if (trigger == "falling_edge") {
        trg_mode = 1;
    } else if (trigger == "both") {
        trg_mode = 2;
    }

    uint32_t acq_mode = 0;
    if (mode == "continuous") {
        acq_mode = 0;
    } else if (mode == "single") {
        acq_mode = 1;
    } else if (mode == "free_running") {
        acq_mode = 2;
    }

    hw.write_direct(scope_internal_addr + am.scope_int.trg_mode, trg_mode);
    hw.write_direct(scope_internal_addr + am.scope_int.trg_src, source - 1);
    hw.write_direct(scope_internal_addr + am.scope_int.trg_lvl, float_to_uint32(level));
    hw.write_direct(scope_internal_addr + am.scope_int.acq_mode, acq_mode);
    hw.write_direct(scope_internal_addr + am.scope_int.trg_point, trigger_point);
    if (prescaler > 2) {
        hw.write_direct(scope_base_address + am.tb_base + am.tb.ctrl, 1);
        hw.write_direct(scope_base_address + am.tb_base + am.tb.period, prescaler);
        hw.write_direct(scope_base_address + am.tb_base + am.tb.threshold, 1);
    }
}

std::string scope_controller::get_acquisition_status() {
    if (scope_base_address == 0) return "not present";
    switch (hw.read_direct(scope_base_address + am.internal_base + am.scope_int.trg_rearm_status)) {
        case 0:
            return "wait";
        case 1:
            return "run";
        case 2:
            return "stop";
        case 3:
            return "free run";
        default:
            return "unknown";
    }
}

void scope_controller::set_scope_address(const nlohmann::json &args) {
    if (args.is_object() && args.contains("address") && args["address"].is_null()) return;

    const uint64_t address = unsigned_argument(args, "address");
    const uint64_t buffer_offset = unsigned_argument(args, "dma_buffer_offset");
    spdlog::info("SET SCOPE ADDRESS: {0:x}", address);
    hw.set_scope_data(address + buffer_offset);
    scope_base_address = address;
}

void scope_controller::disable_dma(const nlohmann::json &args) {
    const bool status = required(args, "status");

    // the first request is the one issued while loading, when nothing is configured yet
    if (first_load) {
        first_load = false;
        return;
    }
    spdlog::info(status ? "DISABLE_SCOPE_DMA" : "ENABLE_SCOPE_DMA");

    const auto write_addr = scope_base_address + am.mux_base + am.mux.ctrl;
    if (write_addr != 0) {
        hw.write_direct(write_addr, status);
        usleep(50'000);
    }
}
