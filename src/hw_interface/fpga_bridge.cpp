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

fpga_bridge::fpga_bridge() {

    spdlog::info("fpga_bridge initialization started");

    arch = std::getenv("ARCH");

    if(!runtime_config.emulate_hw){
        busses = bus_accessor();
    } else {
        busses = bus_sink();
    }

    spdlog::info("fpga_bridge initialization done");
}

/// This method loads a bitstrem by name through the linux kernel fpga_manager interface
/// \param bitstream Name of the bitstream to load
/// \return #RESP_OK if the file is found #RESP_ERR_BITSTREAM_NOT_FOUND otherwise
responses::response_code fpga_bridge::load_bitstream(const std::string& bitstream) {

    spdlog::info("LOAD BITSTREAM: {0}", bitstream);
    if(!runtime_config.emulate_hw){
        std::ofstream ofs("/sys/class/fpga_manager/fpga0/flags");
        ofs << "0";
        ofs.flush();
        std::string prefix = "/lib/firmware";
        std::string file = bitstream.substr(prefix.length());

        if(std::filesystem::exists(bitstream)){
            ofs = std::ofstream("/sys/class/fpga_manager/fpga0/firmware");
            ofs << file;
            ofs.flush();

            std::string state;
            std::ifstream ifs("/sys/class/fpga_manager/fpga0/state");
            int timeout_counter = 500;
            do {
                std::this_thread::sleep_for(5ms);
                ifs >> state;
                timeout_counter--;
            } while (state != "operating" && timeout_counter>=0);

            if(timeout_counter <0){
                spdlog::error("Bitstream load failed to complete in time");
                return responses::bitstream_load_failed;
            } else{
                return responses::ok;
            }
        } else {
            spdlog::error("Bitstream not found {0}", file);
            return responses::bitstream_not_found;
        }
    } else{
        return responses::ok;
    }
}

/// Write to a single register
/// \param address Address to write to
/// \param value Value to write
/// \return #RESP_OK
responses::response_code fpga_bridge::single_write_register(const nlohmann::json &write_obj) {

    if(write_obj["type"] == "direct"){
        write_direct(write_obj["address"], write_obj["value"]);
    } else if(write_obj["type"] == "proxied") {
         if(write_obj["proxy_type"] == "axis_constant"){
             write_proxied(write_obj["proxy_address"], write_obj["address"], write_obj["value"]);
        } else {
             throw std::runtime_error("ERROR: Unrecognised write proxy type");
         }
    } else {
        throw std::runtime_error("ERROR: Unrecognised register write type");
    }

    return responses::ok;
}




/// Read a single register
/// \param address Address of the register to read
/// \param value Pointer where the read value will be put
/// \return #RESP_OK
nlohmann::json fpga_bridge::single_read_register(uint64_t address) {
    nlohmann::json response_body;
    spdlog::info("READ SINGLE REGISTER: addr 0x{0:x}", address);
    response_body["data"] = read_direct(address);
    response_body["response_code"] = responses::as_integer(responses::ok);
    return response_body;
}


/// Load a program into the specified fCore instance through the AXI bus
/// \param address Address of the fCore instance
/// \param program Vector with the instructions of the program to load
/// \return #RESP_OK
responses::response_code fpga_bridge::apply_program(uint64_t address, std::vector<uint32_t> program) {
    spdlog::info("APPLY PROGRAM: address:  0x{0:x} program_size: {1}", address, program.size());

    std::visit( [&address, &program](auto& x) { x.load_program(address,program); },busses );
    return responses::ok;
}

responses::response_code fpga_bridge::set_clock_frequency(std::vector<uint32_t> freq) {
    set_pl_clock(freq[0], freq[1]);
    return responses::ok;
}


responses::response_code fpga_bridge::apply_filter(uint64_t address, std::vector<uint32_t> taps) {
    spdlog::info("APPLY FILTER: address: 0x{0:x}  N. Filter Taps {1}", address, taps.size());

    if(!runtime_config.emulate_hw) {
        for(int i = 0; i< taps.size(); i++){
            write_direct(address, taps[i]);
            write_direct(address + 4, i);
        }
    }

    return responses::ok;
}

std::string fpga_bridge::get_module_version() {
    return "KERNEL MODULE VERSIONING NOT IMPLEMENTED YET";
}

std::string fpga_bridge::get_hardware_version() {
    return "HARDWARE VERSIONING NOT IMPLEMENTED YET";
}

responses::response_code fpga_bridge::set_scope_data(uint64_t addr) {
    uint64_t buffer;

    if(!runtime_config.emulate_hw){
        std::ifstream fs(if_dict.get_buffer_address_if());
        fs >> buffer;
    } else {
        buffer = 0xFFCCFFCC;
    }

    spdlog::info("SET_SCOPE_BUFFER_ADDRESS: writing buffer address 0x{0:x} to scope at address 0x{1:x}", buffer, addr);
    write_direct(addr, buffer);
    return responses::ok;
}

void fpga_bridge::write_direct(uint64_t addr, uint32_t val) {
    spdlog::info("WRITE SINGLE REGISTER (DIRECT): addr 0x{0:x} value {1}", addr, val);

    std::visit( [&addr, &val](auto& x) { x.write_register({addr}, val); },busses );
}

void fpga_bridge::write_proxied(uint64_t proxy_addr, uint32_t target_addr, uint32_t val) {
    spdlog::info("WRITE SINGLE REGISTER (AXIS PROXIED): proxy_addr 0x{0:x} addr 0x{1:x} value {2}",proxy_addr, target_addr, val);

    std::visit( [&proxy_addr, &target_addr, &val](auto& x) { x.write_register({target_addr, proxy_addr}, val); },busses );
}

uint32_t fpga_bridge::read_direct(uint64_t address) {
    return std::visit( [&address](auto& x) {return x.read_register({address});},busses );
}

uint32_t fpga_bridge::get_pl_clock( uint8_t clk_n) {
    return 99'999'999;
}

void fpga_bridge::set_pl_clock(uint8_t clk_n, uint32_t freq) {
    std::string command;

    spdlog::info("SET_CLOCK FREQUENCY: clock #{0}  to {1}Hz", clk_n, freq);

    auto fd = open(if_dict.get_clock_if(clk_n).c_str(), O_RDWR | O_DIRECT);

    std::string f_str = std::to_string(freq);
    write(fd, f_str.c_str(), f_str.size());
}

std::vector<bus_op> fpga_bridge::get_bus_operations() {
    return std::get<bus_sink>(busses).get_operations();
}
