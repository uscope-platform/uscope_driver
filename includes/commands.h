// Copyright (C) 2020 Filippo Savi - All Rights Reserved

// This file is part of uscope_driver.

// uscope_driver is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the
// License.

// uscope_driver is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>.

#ifndef USCOPE_DRIVER_COMMANDS_H
#define USCOPE_DRIVER_COMMANDS_H

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

/// This response is issued when the command action has been performed successfully
#define RESP_OK 1
/// The bitstream file specified for the load bitstream command was not found
#define RESP_ERR_BITSTREAM_NOT_FOUND 2
/// No new data to be read
#define RESP_DATA_NOT_READY 3

/// The response for the command is transmitted through the pub/sub channel
#define RESP_TYPE_INBAND 1
/// The response is transmitted through shared files.
#define RESP_TYPE_OUTBAND 2
/// The command does not need a response
#define RESP_NOT_NEEDED 3

/// This structure is used to represent a parsed command
typedef struct {
    uint32_t opcode; /// opcode that indicates what type of command has been sent
    char *operand_1; /// first command specific data field
    char *operand_2; /// second command specific data field
} command_t;

/// This structure contains a response to a command
typedef struct {
    uint32_t opcode; /// opcode of the command
    uint32_t type;  /// type of the response
    uint32_t return_code; /// command completion code
    uint32_t *body; /// optional body of the response
    uint32_t body_size; /// size of the response body
} response_t;


#endif //USCOPE_DRIVER_COMMANDS_H
