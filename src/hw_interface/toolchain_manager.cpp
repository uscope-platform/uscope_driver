

//   Copyright 2024 Filippo Savi <filssavi@gmail.com>
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

#include "hw_interface/toolchain_manager.hpp"


fcore::result<std::vector<uint32_t>>
toolchain_manager::compile(std::string content, nlohmann::json h, nlohmann::json io) {
    // the malformed headers and io maps are reported by the json library with exceptions
    return fcore::catching([&]() -> fcore::result<std::vector<uint32_t>> {
        std::vector<std::string> c = {std::move(content)};

        std::vector<std::string> headers;
        for(auto &item:h){
            headers.push_back(item["content"]);
        }

        fcore::fcore_cc cc(c, headers);
        auto map = fcore::fcore_cc::load_iom_map(io);
        cc.set_dma_map(map);
        if(auto r = cc.compile(1); !r) return fcore::fail(std::move(r).error());
        return cc.get_executable();
    });
}
