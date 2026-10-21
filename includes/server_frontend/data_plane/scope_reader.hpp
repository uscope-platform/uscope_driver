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


#ifndef USCOPE_DRIVER_SCOPE_READER_HPP
#define USCOPE_DRIVER_SCOPE_READER_HPP

#include <functional>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>

#include "hw_interface/bus/scope_accessor.hpp"
#include "hw_interface/scope_scaling.hpp"
#include "server_frontend/data_plane/scope_frame.hpp"

/// Reads the scope data from the board on its own thread and publishes it as ready to send frames
/// (a scope_frame_header followed by the raw data words). It knows nothing about who consumes the frames.
class scope_reader {
public:
    /// Called from the reader thread for every frame read
    using frame_sink = std::function<void(scope_frame)>;

    /// Starts the reader thread, which is stopped and joined by the destructor
    scope_reader(std::shared_ptr<scope_accessor> sa, std::shared_ptr<scope_scaling> scaling, frame_sink sink);
    ~scope_reader();

    scope_reader(const scope_reader &) = delete;
    scope_reader &operator=(const scope_reader &) = delete;

private:
    void read_loop(std::stop_token stop);

    std::shared_ptr<scope_accessor> scope_if;
    std::shared_ptr<scope_scaling> scaling;
    frame_sink sink;
    std::jthread thread;  // last, so that it is started after and joined before the other members
};

#endif //USCOPE_DRIVER_SCOPE_READER_HPP
