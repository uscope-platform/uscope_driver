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

#include "hw_interface/fpga_bridge.hpp"


using namespace std::chrono_literals;

std::atomic<bool> fpga_bridge::fpga_loaded{false};

fpga_bridge::fpga_bridge() {

    auto state_if = if_dict.get_fpga_state_if();
    if(state_if){
        std::ifstream ifs(state_if.value());
        if(ifs.get() == '1'){
            fpga_loaded = true;
            return;
        }
    } else {
        spdlog::warn("Unable to check whether the FPGA has been loaded: {0}", state_if.error().what());
    }
    spdlog::info("FPGA HAS NOT BEEN LOADED YET, ALL AXI ACCESSES ARE DIABLED");
}

/// This method loads a bitstrem by name through the linux kernel fpga_manager interface
/// \param bitstream Name of the bitstream to load
/// \return #RESP_OK if the file is found #RESP_ERR_BITSTREAM_NOT_FOUND otherwise
fcore::result<responses::response_code> fpga_bridge::load_bitstream(const std::vector<uint8_t>& bitstream) {

    spdlog::warn("LOAD BITSTREAM: loading file");
    auto driver = if_dict.get_fpga_bitstream_if();
    if(!driver) return fcore::fail(std::move(driver).error());
    const int fd = open(driver.value().c_str(), O_RDWR);

    write(fd, bitstream.data(), bitstream.size());

    spdlog::warn("LOAD BITSTREAM: written file");

    auto ret = ioctl(fd, 3);

    if(ret <0) return responses::bitstream_load_failed;

    char status;
    read(fd, &status, 1);

    if(status == '0') return responses::bitstream_load_failed;

    spdlog::info("LOAD BITSTREAM: bitstream loaded");

    fpga_loaded = true;
    return responses::ok;

}

/// Write to a single register
/// \param address Address to write to
/// \param value Value to write
/// \return #RESP_OK
fcore::result<responses::response_code> fpga_bridge::single_write_register(const nlohmann::json &write_obj) {

    // the fields of the object are read with the json library, which reports a missing or mistyped one with an exception
    return fcore::catching([&]() -> fcore::result<responses::response_code> {
        fcore::result<void> written;
        if(write_obj["type"] == "direct"){
            written = write_direct(write_obj["address"], write_obj["value"]);
        } else if(write_obj["type"] == "proxied") {
            if(write_obj["proxy_type"] == "axis_constant"){
                written = write_proxied(write_obj["proxy_address"], write_obj["address"], write_obj["value"]);
            } else {
                return fcore::fail(fcore::error_code::invalid_argument, "ERROR: Unrecognised write proxy type");
            }
        } else {
            return fcore::fail(fcore::error_code::invalid_argument, "ERROR: Unrecognised register write type");
        }
        if(!written) return fcore::fail(std::move(written).error());
        return responses::ok;
    });
}


/// Read a single register
/// \param address Address of the register to read
/// \param value Pointer where the read value will be put
/// \return #RESP_OK
fcore::result<nlohmann::json> fpga_bridge::single_read_register(uint64_t address) {
    nlohmann::json response_body;
    spdlog::info("READ SINGLE REGISTER: addr 0x{0:x}", address);
    auto value = read_direct(address);
    if(!value) return fcore::fail(std::move(value).error());
    response_body["data"] = value.value();
    response_body["response_code"] = responses::as_integer(responses::ok);
    return response_body;
}


/// Load a program into the specified fCore instance through the AXI bus
/// \param address Address of the fCore instance
/// \param program Vector with the instructions of the program to load
/// \return #RESP_OK
fcore::result<responses::response_code> fpga_bridge::apply_program(uint64_t address, std::vector<uint32_t> program) {
    spdlog::info("APPLY PROGRAM: address:  0x{0:x} program_size: {1}", address, program.size());
    if(auto loaded = busses->load_program(address, program); !loaded) return fcore::fail(std::move(loaded).error());
    return responses::ok;
}



fcore::result<responses::response_code> fpga_bridge::apply_filter(uint64_t address, std::vector<uint32_t> taps) {
    spdlog::info("APPLY FILTER: address: 0x{0:x}  N. Filter Taps {1}", address, taps.size());

    for(int i = 0; i< taps.size(); i++){
        if(auto written = write_direct(address, taps[i]); !written) return fcore::fail(std::move(written).error());
        if(auto written = write_direct(address + 4, i); !written) return fcore::fail(std::move(written).error());
    }

    return responses::ok;
}

std::string fpga_bridge::get_module_version() {
    return "KERNEL MODULE VERSIONING NOT IMPLEMENTED YET";
}

std::string fpga_bridge::get_hardware_version() {
    return "HARDWARE VERSIONING NOT IMPLEMENTED YET";
}

fcore::result<responses::response_code> fpga_bridge::set_scope_data(uint64_t addr) {
    uint64_t buffer;

    auto buffer_if = if_dict.get_buffer_address_if();
    if(!buffer_if) return fcore::fail(std::move(buffer_if).error());
    std::ifstream fs(buffer_if.value());
    if(!(fs >> buffer)) return fcore::fail(fcore::error_code::io_error, "Unable to read the address of the scope buffer from " + buffer_if.value());

    spdlog::info("SET_SCOPE_BUFFER_ADDRESS: writing buffer address 0x{0:x} to scope at address 0x{1:x}", buffer, addr);
    if(auto written = write_direct(addr, buffer); !written) return fcore::fail(std::move(written).error());
    return responses::ok;
}

fcore::result<void> fpga_bridge::write_direct(uint64_t addr, uint32_t val) {
    spdlog::info("WRITE SINGLE REGISTER (DIRECT): addr 0x{0:x} value {1}", addr, val);
    if(fpga_loaded) return busses->write_register({addr}, val);
    return {};
}

fcore::result<void> fpga_bridge::write_proxied(uint64_t proxy_addr, uint32_t target_addr, uint32_t val) {
    spdlog::info("WRITE SINGLE REGISTER (AXIS PROXIED): proxy_addr 0x{0:x} addr 0x{1:x} value {2}",proxy_addr, target_addr, val);
    if(fpga_loaded) return busses->write_register({target_addr, proxy_addr}, val);
    return {};
}

fcore::result<uint32_t> fpga_bridge::read_direct(uint64_t address) {
    spdlog::info("READ SINGLE REGISTER (DIRECT): addr 0x{0:x}", address);
    if(!fpga_loaded) return 0;
    std::vector<uint64_t> a= {address};
    return busses->read_register(a);
}

uint32_t fpga_bridge::get_pl_clock( uint8_t clk_n) {
    return 99'999'999;
}

fcore::result<responses::response_code> fpga_bridge::set_pl_clock(uint8_t clk_n, uint32_t freq) {
    std::string command;

    spdlog::info("SET_CLOCK FREQUENCY: clock #{0}  to {1}Hz", clk_n, freq);

    auto clock_if = if_dict.get_clock_if(clk_n);
    if(!clock_if) return fcore::fail(std::move(clock_if).error());
    std::string file = clock_if.value();
    auto fd = open(file.c_str(), O_RDWR | O_SYNC);
    if(fd<0){
        spdlog::critical("Failed to open the clock setting file: {0}", file);
        spdlog::critical("Clock setting operation unsuccessful: {0}", std::strerror(errno));
        return responses::driver_file_not_found;
    }

    std::string f_str = std::to_string(freq);
    auto f_cstr = f_str.c_str();
    ssize_t write_size = f_str.size();
    auto res = write(fd,f_cstr, write_size);
    if(res<0){
        spdlog::critical("WRITE FAILURE: Clock setting operation unsuccessful: {0}", std::strerror(errno));
        return responses::driver_write_failed;
    }
    return responses::ok;
}

std::vector<bus_op> fpga_bridge::get_bus_operations() const {
    return busses->get_operations();
}

std::pair<std::string, std::string> fpga_bridge::get_hardware_simulation_data() const {
    return busses->get_hardware_simulation_data();
}
