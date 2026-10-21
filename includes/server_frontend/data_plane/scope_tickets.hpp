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

#ifndef USCOPE_DRIVER_SCOPE_TICKETS_HPP
#define USCOPE_DRIVER_SCOPE_TICKETS_HPP

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

/// One time tickets that allow a client to open a scope websocket. They are registered through the control plane
/// (which is authenticated by the uscope server) and consumed by the websocket server when a client connects.
/// Registration and consumption happen on different threads.
class scope_tickets {
public:
    using clock = std::chrono::steady_clock;

    void add(const std::string &ticket, const std::string &role, std::chrono::seconds ttl) {
        std::lock_guard lock(mutex);
        const auto now = clock::now();
        std::erase_if(entries, [now](const auto &e){ return e.second.expiry <= now; });
        entries[ticket] = {role, now + ttl};
    }

    /// Invalidates the ticket
    /// \return the role it was issued for, if the ticket exists and is not expired
    std::optional<std::string> consume(const std::string &ticket) {
        std::lock_guard lock(mutex);
        auto it = entries.find(ticket);
        if (it == entries.end()) return std::nullopt;
        auto entry = std::move(it->second);
        entries.erase(it);
        if (entry.expiry <= clock::now()) return std::nullopt;
        return entry.role;
    }
private:
    struct entry {
        std::string role;
        clock::time_point expiry;
    };
    std::mutex mutex;
    std::unordered_map<std::string, entry> entries;
};

#endif //USCOPE_DRIVER_SCOPE_TICKETS_HPP
