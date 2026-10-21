

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

#ifndef USCOPE_DRIVER_SCOPE_SERVER_HPP
#define USCOPE_DRIVER_SCOPE_SERVER_HPP



#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "hw_interface/bus/scope_accessor.hpp"
#include "server_frontend/data_plane/scope_frame.hpp"

class scope_session;

/// Websocket server pushing the newest scope data to all the connected clients. Clients that can not keep up
/// skip frames, they never receive stale data.
class scope_server {
    public:
        scope_server(int port, std::shared_ptr<scope_accessor> sa);
        ~scope_server();
        /// Runs the event loop until a stop is requested through the token
        void run(std::stop_token stop);
    private:
        boost::asio::awaitable<void> listen();
        void publish(scope_frame f);
        void broadcast(const scope_frame &f);

        int port;
        std::shared_ptr<scope_accessor> scope_if;

        std::mutex latest_mutex;
        scope_frame latest_frame;
        std::atomic<bool> broadcast_scheduled{false};

        boost::asio::io_context io_context;
        std::vector<std::weak_ptr<scope_session>> sessions;
};


#endif //USCOPE_DRIVER_SCOPE_SERVER_HPP
