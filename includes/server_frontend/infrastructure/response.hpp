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

#ifndef USCOPE_DRIVER_RESPONSE_HPP
#define USCOPE_DRIVER_RESPONSE_HPP

#include <cstdint>
#include <vector>


namespace responses {

    typedef enum {
        ok = 1,
        bitstream_not_found = 2,
        invalid_cmd_schema = 3,
        invalid_arg = 4,
        bitstream_load_failed =5,
        internal_erorr = 6,
        emulation_error = 7,
        deployment_error= 8,
        hil_bus_conflict_warning = 9,
        driver_file_not_found = 10,
        driver_write_failed = 11
    } response_code;

    template<typename response_code>
    auto as_integer(response_code const value)
    -> typename std::underlying_type<response_code>::type {
        return static_cast<typename std::underlying_type<response_code>::type>(value);
    }

}

#endif //USCOPE_DRIVER_RESPONSE_HPP
