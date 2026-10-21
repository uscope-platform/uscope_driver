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

#include <boost/beast/http.hpp>
#include <spdlog/spdlog.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;
using asio::ip::tcp;

scope_session::scope_session(tcp::socket socket, std::shared_ptr<scope_tickets> tickets,
                             std::shared_ptr<scope_controller> controller)
    : ws(std::move(socket)), wake(ws.get_executor()), tickets(std::move(tickets)), controller(std::move(controller)) {

}

/// Value of the ticket query parameter of the request target, empty if absent
static std::string ticket_of(std::string_view target) {
    constexpr std::string_view key = "ticket=";
    for (size_t pos = target.find('?'); pos != std::string_view::npos; pos = target.find('&', pos + 1)) {
        auto param = target.substr(pos + 1);
        if (param.starts_with(key)) return std::string(param.substr(key.size(), param.find('&') - key.size()));
    }
    return {};
}

void scope_session::push(const scope_frame &f) {
    pending = f;
    wake.cancel();
}

void scope_session::send_reply(std::string reply) {
    if (closed) return;
    replies.push_back(std::move(reply));
    wake.cancel();
}

asio::awaitable<void> scope_session::run(std::shared_ptr<scope_session> self) {
    try {
        // the upgrade request is checked before anything else is done with the connection
        beast::flat_buffer request_buffer;
        http::request<http::string_body> request;
        beast::get_lowest_layer(self->ws).expires_after(std::chrono::seconds(30));
        co_await http::async_read(beast::get_lowest_layer(self->ws), request_buffer, request, asio::use_awaitable);
        beast::get_lowest_layer(self->ws).expires_never();

        auto role = websocket::is_upgrade(request) ? self->tickets->consume(ticket_of(request.target()))
                                                   : std::nullopt;
        if (!role) {
            http::response<http::string_body> denied{http::status::unauthorized, request.version()};
            denied.set(http::field::content_type, "text/plain");
            denied.body() = "Invalid or expired scope ticket";
            denied.prepare_payload();
            denied.keep_alive(false);
            co_await http::async_write(beast::get_lowest_layer(self->ws), denied, asio::use_awaitable);
            self->close();
            co_return;
        }
        self->role = *role;

        self->ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        self->ws.binary(true);
        self->ws.auto_fragment(false);
        self->ws.read_message_max(max_command_size);
        co_await self->ws.async_accept(request, asio::use_awaitable);

        asio::co_spawn(self->ws.get_executor(), write_loop(self), asio::detached);

        // the data is only pushed to the client, the incoming text messages are the scope commands, one at a time.
        // The read loop also processes the control frames (ping/pong/close) and detects disconnections
        beast::flat_buffer buffer;
        while (true) {
            buffer.clear();
            co_await self->ws.async_read(buffer, asio::use_awaitable);
            if (self->ws.got_text()) co_await handle_command(self, beast::buffers_to_string(buffer.data()));
        }
    } catch (const boost::system::system_error &e) {
        report(e);
    }
    self->close();
}

/// Executes a command of the client, replying with {"id": ..., "ok": ..., "data" | "error": ...}
asio::awaitable<void> scope_session::handle_command(std::shared_ptr<scope_session> self, std::string message) {
    auto request = nlohmann::json::parse(message, nullptr, false);

    nlohmann::json reply;
    if (request.is_object() && request.contains("cmd") && request["cmd"].is_string()) {
        reply = co_await self->controller->execute(request["cmd"], request.value("args", nlohmann::json()));
    } else {
        reply = {{"ok", false}, {"error", "DRIVER ERROR: malformed scope command"}};
    }
    if (request.is_object() && request.contains("id")) reply["id"] = request["id"];
    self->send_reply(reply.dump());
}

asio::awaitable<void> scope_session::write_loop(std::shared_ptr<scope_session> self) {
    try {
        while (!self->closed) {
            if (!self->replies.empty()) {
                auto reply = std::move(self->replies.front());
                self->replies.pop_front();
                self->ws.text(true);
                co_await self->ws.async_write(asio::buffer(reply), asio::use_awaitable);
                continue;
            }
            if (!self->pending) {
                // woken up by push(), send_reply() or close()
                self->wake.expires_at(asio::steady_timer::time_point::max());
                boost::system::error_code ignored;
                co_await self->wake.async_wait(asio::redirect_error(asio::use_awaitable, ignored));
                continue;
            }
            auto f = std::move(self->pending);
            self->pending.reset();
            self->ws.binary(true);
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
    replies.clear();
    wake.cancel();
    boost::system::error_code ignored;
    beast::get_lowest_layer(ws).socket().close(ignored);
}
