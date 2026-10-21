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


#include "server_frontend/data_plane/scope_session.hpp"

#include <spdlog/spdlog.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
using asio::ip::tcp;

scope_session::scope_session(tcp::socket socket) : ws(std::move(socket)), wake(ws.get_executor()) {

}

void scope_session::push(const scope_frame &f) {
    pending = f;
    wake.cancel();
}

asio::awaitable<void> scope_session::run(std::shared_ptr<scope_session> self) {
    try {
        self->ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        self->ws.binary(true);
        self->ws.auto_fragment(false);
        co_await self->ws.async_accept(asio::use_awaitable);

        asio::co_spawn(self->ws.get_executor(), write_loop(self), asio::detached);

        // data is only pushed to the client, incoming messages are discarded; the read loop is needed to
        // process the control frames (ping/pong/close) and to detect disconnections
        beast::flat_buffer buffer;
        while (true) {
            buffer.clear();
            co_await self->ws.async_read(buffer, asio::use_awaitable);
        }
    } catch (const boost::system::system_error &e) {
        report(e);
    }
    self->close();
}

asio::awaitable<void> scope_session::write_loop(std::shared_ptr<scope_session> self) {
    try {
        while (!self->closed) {
            if (!self->pending) {
                // woken up by push() or close()
                self->wake.expires_at(asio::steady_timer::time_point::max());
                boost::system::error_code ignored;
                co_await self->wake.async_wait(asio::redirect_error(asio::use_awaitable, ignored));
                continue;
            }
            auto f = std::move(self->pending);
            self->pending.reset();
            co_await self->ws.async_write(asio::buffer(*f), asio::use_awaitable);
        }
    } catch (const boost::system::system_error &e) {
        report(e);
    }
    self->close();
}

void scope_session::report(const boost::system::system_error &e) {
    if (e.code() != websocket::error::closed && e.code() != asio::error::eof
        && e.code() != asio::error::not_connected && e.code() != asio::error::connection_reset
        && e.code() != asio::error::operation_aborted) {
        spdlog::warn("Scope data client disconnected: {0}", e.what());
    }
}

void scope_session::close() {
    closed = true;
    pending.reset();
    wake.cancel();
    boost::system::error_code ignored;
    beast::get_lowest_layer(ws).socket().close(ignored);
}
