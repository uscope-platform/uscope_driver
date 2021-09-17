// Copyright 2021 University of Nottingham Ningbo China
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef USCOPE_DRIVER_COMMANDS_HHPP
#define USCOPE_DRIVER_COMMANDS_HHPP

#include <stdint.h>
#include <stdlib.h>

/// Command with no effects
/// EXPECTED RESPONSE TYPE: #RESP_NOT_NEEDED
#define C_NULL_COMMAND 0

/// Load a bitstream by name
/// FORMAT: C_LOAD_BITSTREAM name
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_LOAD_BITSTREAM 1

/// Write to a single register
/// FORMAT: C_SINGLE_REGISTER_WRITE address value
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SINGLE_REGISTER_WRITE 2

/// Write to multiple registers in bulk
/// FORMAT: C_BULK_REGISTER_WRITE address_1, address_2,... value_1, value_2,...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_BULK_REGISTER_WRITE 3

/// Read from a single register
/// FORMAT: C_SINGLE_REGISTER_READ address
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SINGLE_REGISTER_READ 4

/// Read from multiple registers in bulk
/// FORMAT: C_BULK_REGISTER_READ address_1, address_2,...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_BULK_REGISTER_READ 5

/// Start capture mode
/// FORMAT: C_START_CAPTURE n_buffers
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_START_CAPTURE 6

/// Write to a register through a Proxy
/// FORMAT: C_PROXIED_WRITE proxy_address,register_address value
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_PROXIED_WRITE 7

/// Read captured data
/// FORMAT: C_READ_DATA
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_OUTBAND
#define C_READ_DATA 8

/// Check how many buffers are there left to capture
/// FORMAT: C_CHECK_CAPTURE_PROGRESS
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_CHECK_CAPTURE_PROGRESS 9


/// Check how many buffers are there left to capture
/// FORMAT: C_SET_CHANNEL_WIDTHS ch_1_width,ch2_width,...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SET_CHANNEL_WIDTHS 10



/// Load a program into the specified fCore instance memory
/// FORMAT: C_APPLY_PROGRAM fCore_address program_instruction_1, program_instruction_2, ...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_APPLY_PROGRAM 11



/// This response is issued when the command action has been performed successfully
#define RESP_OK 1
/// The bitstream file specified for the load bitstream command was not found
#define RESP_ERR_BITSTREAM_NOT_FOUND 2






#endif //USCOPE_DRIVER_COMMANDS_HHPP

