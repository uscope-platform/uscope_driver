

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

#include "server_frontend/data_plane/scope_server.hpp"
#include "server_frontend/data_plane/scope_frame.hpp"
#include "server_frontend/data_plane/scope_reader.hpp"
#include "server_frontend/data_plane/scope_session.hpp"
#include "emulation/emulated_scope_reader.hpp"
#include "emulation/emulation_context.hpp"

#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
using asio::ip::tcp;

scope_server::scope_server(int port, std::shared_ptr<bus_accessor> ba, std::shared_ptr<scope_accessor> sa,
                           std::shared_ptr<scope_tickets> tickets)
    : port(port), scope_if(std::move(sa)), scaling(std::make_shared<scope_scaling>()), tickets(std::move(tickets)),
      controller(std::make_shared<scope_controller>(std::move(ba), scaling)) {

}

scope_server::~scope_server(){
    spdlog::info("Scope data server stopping...");
}

/// Accepts connections and spawns a session for each one
asio::awaitable<void> scope_server::listen() {
    auto executor = co_await asio::this_coro::executor;
    tcp::acceptor acceptor(executor, tcp::endpoint(tcp::v4(), port));
    acceptor.set_option(asio::socket_base::reuse_address(true));

    while (true) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);

        // the data is latency sensitive, so keep the amount of stale data buffered by the kernel small
        socket.set_option(tcp::no_delay(true));
        socket.set_option(asio::socket_base::send_buffer_size(
            static_cast<int>(2 * (sizeof(scope_frame_header) + scope_accessor::n_channels * scope_accessor::buffer_size * sizeof(float)))));

        auto session = std::make_shared<scope_session>(std::move(socket), tickets, controller);
        std::erase_if(sessions, [](const auto &s){ return s.expired(); });
        sessions.push_back(session);
        asio::co_spawn(executor, scope_session::run(session), asio::detached);
    }
}

/// Sends the frame to all the clients, runs in the event loop thread
void scope_server::broadcast(const scope_frame &f) {
    for (auto &weak_session : sessions) {
        if (auto session = weak_session.lock()) session->push(f);
    }
}

/// Called from the reader thread. At most one broadcast is scheduled at any time and it always sends the
/// newest frame, so a slow event loop can not accumulate old data.
void scope_server::publish(scope_frame f) {
    {
        std::lock_guard lock(latest_mutex);
        latest_frame = std::move(f);
    }

    if (!broadcast_scheduled.exchange(true)) {
        asio::post(io_context, [this] {
            broadcast_scheduled = false;
            scope_frame newest;
            {
                std::lock_guard lock(latest_mutex);
                newest = std::move(latest_frame);
            }
            if (newest) broadcast(newest);
        });
    }
}

void scope_server::run(std::stop_token stop) {
    spdlog::info("Scope data server starting...");

    // wakes the event loop from the requesting thread, also if the stop was already requested
    std::stop_callback stop_loop(stop, [this]{ io_context.stop(); });

    asio::co_spawn(io_context, listen(), [](std::exception_ptr e) {
        if (e) {
            try {
                std::rethrow_exception(e);
            } catch (const std::exception &ex) {
                spdlog::error("Scope data server failed: {0}", ex.what());
            }
        }
    });

    // stopped (and joined) when this function returns, whatever the reason the event loop ended
    // both the board and the emulation produce frames, only the ones of the target are published
    auto &emulation = emulation_context::instance();
    scope_reader reader(scope_if, scaling, [this, &emulation](scope_frame f){
        if(!emulation.emulator_selected()) publish(std::move(f));
    });
    emulated_scope_reader emulated_reader(emulation.get_emulation(), scaling, [this, &emulation](scope_frame f){
        if(emulation.emulator_selected()) publish(std::move(f));
    });

    io_context.run();
}
