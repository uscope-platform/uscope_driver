

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

#ifndef USCOPE_DRIVER_CONFIGURATION_HPP
#define USCOPE_DRIVER_CONFIGURATION_HPP

class configuration {
public:
    bool emulate_hw;
    bool debug_hil;
    unsigned int server_port;
};

extern configuration runtime_config;

#endif //USCOPE_DRIVER_CONFIGURATION_HPP
