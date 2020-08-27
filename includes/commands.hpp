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

/// Set status for all channels, in the format for the channels 1 is enabled and 0 is disabled
/// FORMAT: C_SET_CHANNEL_STATUS ch1_status ch2_status ch3_status
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SET_CHANNEL_STATUS 10



/// This response is issued when the command action has been performed successfully
#define RESP_OK 1
/// The bitstream file specified for the load bitstream command was not found
#define RESP_ERR_BITSTREAM_NOT_FOUND 2
/// No new data to be read
#define RESP_DATA_NOT_READY 3





#endif //USCOPE_DRIVER_COMMANDS_HHPP
