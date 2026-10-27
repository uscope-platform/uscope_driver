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
    /// Reads an unsigned integer argument, tolerating the integral floating point values produced by javascript
    fcore::result<uint64_t> unsigned_argument(const nlohmann::json &args, const char *name) {
        if (!args.is_object() || !args.contains(name)) return fcore::fail(fcore::error_code::invalid_argument, std::string("missing argument ") + name);
        const auto &v = args[name];
        if (v.is_number_unsigned()) return v.get<uint64_t>();
        if (v.is_number_integer() && v.get<int64_t>() >= 0) return static_cast<uint64_t>(v.get<int64_t>());
        if (v.is_number_float()) {
            double d = v;
            if (d >= 0 && d == std::floor(d)) return static_cast<uint64_t>(d);
        }
        return fcore::fail(fcore::error_code::invalid_argument, std::string("argument ") + name + " must be an unsigned integer");
    }

    fcore::result<nlohmann::json> required(const nlohmann::json &args, const char *name) {
        if (!args.is_object() || !args.contains(name)) return fcore::fail(fcore::error_code::invalid_argument, std::string("missing argument ") + name);
        return args[name];
    }

    using register_write = std::pair<uint64_t, uint32_t>;

    /// Performs the writes in order, stopping at the first one that fails
    fcore::result<void> write_registers(fpga_bridge &hw, const std::vector<register_write> &writes) {
        for (auto &[address, value] : writes) {
            if (auto written = hw.write_direct(address, value); !written) return written;
        }
        return {};
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
    // the arguments are read with the json library, which reports the ones of the wrong type with exceptions
    auto outcome = fcore::catching([&]() -> fcore::result<void> {
        if (cmd == "set_scaling_factors") {
            return set_scaling_factors(args);
        } else if (cmd == "set_acquisition") {
            return set_acquisition(args);
        } else if (cmd == "get_acquisition_status") {
            auto status = get_acquisition_status();
            if (!status) return fcore::fail(std::move(status).error());
            reply["data"] = status.value();
            return {};
        } else if (cmd == "set_scope_address") {
            return set_scope_address(args);
        } else if (cmd == "disable_scope_dma") {
            return disable_dma(args);
        }
        return fcore::fail(fcore::error_code::invalid_argument, "unknown scope command " + cmd);
    });
    if (!outcome) {
        spdlog::warn("Scope command {0} failed: {1}", cmd, outcome.error().what());
        reply = {{"ok", false}, {"error", std::string("DRIVER ERROR: ") + outcome.error().what()}};
    }
    return reply;
}

fcore::result<void> scope_controller::set_scaling_factors(const nlohmann::json &args) {
    if (!args.is_array() || args.empty()) {
        return fcore::fail(fcore::error_code::invalid_argument, "the scaling factors must be a non empty array");
    }
    if (!std::all_of(args.begin(), args.end(), [](const nlohmann::json &el){ return el.is_number(); })) {
        return fcore::fail(fcore::error_code::invalid_argument, "the scaling factors must be numeric values");
    }
    std::vector<float> factors = args;
    spdlog::info("SET_SCALING_FACTORS: {0}", args.dump());
    scaling->set(factors);
    return {};
}

fcore::result<void> scope_controller::set_acquisition(const nlohmann::json &args) {
    auto trigger_arg = required(args, "trigger");
    if (!trigger_arg) return fcore::fail(std::move(trigger_arg).error());
    auto level_arg = required(args, "level");
    if (!level_arg) return fcore::fail(std::move(level_arg).error());
    auto source_arg = unsigned_argument(args, "source");
    if (!source_arg) return fcore::fail(std::move(source_arg).error());
    auto mode_arg = required(args, "mode");
    if (!mode_arg) return fcore::fail(std::move(mode_arg).error());
    auto trigger_point_arg = unsigned_argument(args, "trigger_point");
    if (!trigger_point_arg) return fcore::fail(std::move(trigger_point_arg).error());
    auto prescaler_arg = unsigned_argument(args, "prescaler");
    if (!prescaler_arg) return fcore::fail(std::move(prescaler_arg).error());
    auto level_type_arg = required(args, "level_type");
    if (!level_type_arg) return fcore::fail(std::move(level_type_arg).error());

    const auto trigger = trigger_arg.value().get<std::string>();
    const float level = level_arg.value();
    const uint64_t source = source_arg.value();
    const auto mode = mode_arg.value().get<std::string>();
    const uint64_t trigger_point = trigger_point_arg.value();
    const uint64_t prescaler = prescaler_arg.value();

    if (scope_base_address == 0) return {};
    if (source == 0) return fcore::fail(fcore::error_code::invalid_argument, "the trigger source is numbered from 1");

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

    auto written = write_registers(hw, {
        {scope_internal_addr + am.scope_int.trg_mode, trg_mode},
        {scope_internal_addr + am.scope_int.trg_src, static_cast<uint32_t>(source - 1)},
        {scope_internal_addr + am.scope_int.trg_lvl, float_to_uint32(level)},
        {scope_internal_addr + am.scope_int.acq_mode, acq_mode},
        {scope_internal_addr + am.scope_int.trg_point, static_cast<uint32_t>(trigger_point)}
    });
    if (!written) return written;
    if (prescaler > 2) {
        return write_registers(hw, {
            {scope_base_address + am.tb_base + am.tb.ctrl, 1},
            {scope_base_address + am.tb_base + am.tb.period, static_cast<uint32_t>(prescaler)},
            {scope_base_address + am.tb_base + am.tb.threshold, 1}
        });
    }
    return {};
}

fcore::result<std::string> scope_controller::get_acquisition_status() {
    if (scope_base_address == 0) return "not present";
    auto status = hw.read_direct(scope_base_address + am.internal_base + am.scope_int.trg_rearm_status);
    if (!status) return fcore::fail(std::move(status).error());
    switch (status.value()) {
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

fcore::result<void> scope_controller::set_scope_address(const nlohmann::json &args) {
    if (args.is_object() && args.contains("address") && args["address"].is_null()) return {};

    auto address = unsigned_argument(args, "address");
    if (!address) return fcore::fail(std::move(address).error());
    auto buffer_offset = unsigned_argument(args, "dma_buffer_offset");
    if (!buffer_offset) return fcore::fail(std::move(buffer_offset).error());
    spdlog::info("SET SCOPE ADDRESS: {0:x}", address.value());
    if (auto set = hw.set_scope_data(address.value() + buffer_offset.value()); !set) return fcore::fail(std::move(set).error());
    scope_base_address = address.value();
    return {};
}

fcore::result<void> scope_controller::disable_dma(const nlohmann::json &args) {
    auto status_arg = required(args, "status");
    if (!status_arg) return fcore::fail(std::move(status_arg).error());
    const bool status = status_arg.value();

    // the first request is the one issued while loading, when nothing is configured yet
    if (first_load) {
        first_load = false;
        return {};
    }
    spdlog::info(status ? "DISABLE_SCOPE_DMA" : "ENABLE_SCOPE_DMA");

    const auto write_addr = scope_base_address + am.mux_base + am.mux.ctrl;
    if (write_addr != 0) {
        if (auto written = hw.write_direct(write_addr, status); !written) return written;
        usleep(50'000);
    }
    return {};
}
