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



    auto raw_arch =  std::getenv("ARCH");
    if(raw_arch== nullptr){
        spdlog::error("Architecture selection (through the ARCH environment variable) is mandatory");
        exit(-2);
    }
    arch = raw_arch;

    if(!runtime_config.emulate_hw) {
        std::string state;
        std::ifstream ifs(if_dict.get_fpga_state_if());
        ifs >> state;
        if(state == "operating"){
            fpga_loaded = true;
        } else {
            spdlog::info("FPGA HAS NOT BEEN LOADED YET, ALL AXI ACCESSES ARE DIABLED");
        }
    } else {
        fpga_loaded = true;
    }

}

/// This method loads a bitstrem by name through the linux kernel fpga_manager interface
/// \param bitstream Name of the bitstream to load
/// \return #RESP_OK if the file is found #RESP_ERR_BITSTREAM_NOT_FOUND otherwise
responses::response_code fpga_bridge::load_bitstream(const std::string& bitstream) {

    spdlog::warn("LOAD BITSTREAM: loading file {0}", bitstream);
    if(!runtime_config.emulate_hw){
        std::ofstream ofs(if_dict.get_fpga_flags_if());
        ofs << "0";
        ofs.flush();

        spdlog::info("LOAD BITSTREAM: flags setup done", bitstream);
        std::string prefix = if_dict.get_firmware_store();
        std::string file = bitstream.substr(prefix.length());

        if(std::filesystem::exists(bitstream)){
            ofs = std::ofstream(if_dict.get_fpga_bitstream_if());
            ofs << file;
            ofs.flush();
            spdlog::info("LOAD BITSTREAM: bitstream loading_started", bitstream);

            std::string state;
            std::ifstream ifs(if_dict.get_fpga_state_if());
            int timeout_counter = 700;
            do {
                std::this_thread::sleep_for(5ms);
                ifs >> state;
                timeout_counter--;
            } while (state != "operating" && timeout_counter>=0);

            spdlog::info("LOAD BITSTREAM: bitstream loaded in {0} ms",5*(700-timeout_counter));

            if(timeout_counter <0){
                spdlog::error("Bitstream load failed to complete in time");
                return responses::bitstream_load_failed;
            } else{
                fpga_loaded = true;
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
    busses->load_program(address, program);
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
    if(fpga_loaded) busses->write_register({addr}, val);
}

void fpga_bridge::write_proxied(uint64_t proxy_addr, uint32_t target_addr, uint32_t val) {
    spdlog::info("WRITE SINGLE REGISTER (AXIS PROXIED): proxy_addr 0x{0:x} addr 0x{1:x} value {2}",proxy_addr, target_addr, val);
    if(fpga_loaded) busses->write_register({target_addr, proxy_addr}, val);
}

uint32_t fpga_bridge::read_direct(uint64_t address) {
    spdlog::info("READ SINGLE REGISTER (DIRECT): addr 0x{0:x}", address);
    if(!fpga_loaded) return 0;
    std::vector<uint64_t> a= {address};
    return busses->read_register(a);
}

uint32_t fpga_bridge::get_pl_clock( uint8_t clk_n) {
    return 99'999'999;
}

responses::response_code fpga_bridge::set_pl_clock(uint8_t clk_n, uint32_t freq) {
    std::string command;

    spdlog::info("SET_CLOCK FREQUENCY: clock #{0}  to {1}Hz", clk_n, freq);

    std::string file = if_dict.get_clock_if(clk_n).c_str();
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
