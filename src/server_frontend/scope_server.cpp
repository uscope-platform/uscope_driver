

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

#include "server_frontend/scope_server.hpp"

#include <spdlog/spdlog.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
using asio::ip::tcp;

scope_server::scope_server(int port) : port(port) {

}

scope_server::~scope_server(){
    spdlog::critical("Scope data server stopping...");
}

/// Accepts connections and spawns a session for each one
asio::awaitable<void> scope_server::listen() {
    auto executor = co_await asio::this_coro::executor;
    tcp::acceptor acceptor(executor, tcp::endpoint(tcp::v4(), port));
    acceptor.set_option(asio::socket_base::reuse_address(true));

    while (true) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        asio::co_spawn(executor, session(std::move(socket)), asio::detached);
    }
}

/// Websocket session, the data is pushed to the client by the server, so incoming messages are discarded;
/// the read loop is only needed to process control frames (ping/pong/close) and detect disconnections
asio::awaitable<void> scope_server::session(tcp::socket socket) {
    try {
        websocket::stream<beast::tcp_stream> ws(std::move(socket));
        ws.binary(true);
        co_await ws.async_accept(asio::use_awaitable);

        beast::flat_buffer buffer;
        while (true) {
            buffer.clear();
            co_await ws.async_read(buffer, asio::use_awaitable);
        }
    } catch (const boost::system::system_error &e) {
        if (e.code() != websocket::error::closed && e.code() != asio::error::eof
            && e.code() != asio::error::not_connected && e.code() != asio::error::connection_reset) {
            spdlog::warn("Scope data client disconnected: {0}", e.what());
        }
    }
}

void scope_server::run(std::stop_token stop) {
    spdlog::critical("Scope data server starting...");

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

    io_context.run();
}
