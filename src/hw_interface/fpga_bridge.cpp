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


volatile int registers_fd, fcore_fd;

using namespace std::chrono_literals;

void sigsegv_handler(int dummy) {
    spdlog::error("Segmentation fault encounteded while communicating with FPGA");
    exit(-1);
}

void sigbus_handler(int dummy) {
    spdlog::error("Bus error encountered while communicating with FPGA");
    exit(-1);
}


fpga_bridge::fpga_bridge() {

    spdlog::info("fpga_bridge initialization started");

    arch = std::getenv("ARCH");

    if(arch == "zynqmp"){
        control_addr = ZYNQMP_REGISTERS_BASE_ADDR;
        core_addr = ZYNQMP_FCORE_BASE_ADDR;
    } else {
        control_addr = ZYNQ_REGISTERS_BASE_ADDR;
        core_addr = ZYNQ_FCORE_BASE_ADDR;
    }


    signal(SIGSEGV,sigsegv_handler);
    signal(SIGBUS,sigbus_handler);


    int n_pages_ctrl, n_pages_fcore;

    char const* t = getenv("MMAP_N_PAGES_CTRL");
    if(t == nullptr){
        n_pages_ctrl = 344000;
    } else {
        n_pages_ctrl = std::stoi(std::string(t));
    }

    t = getenv("MMAP_N_PAGES_FCORE");
    if(t == nullptr){
        n_pages_fcore = 344000;
    } else {
        n_pages_fcore = std::stoi(std::string(t));
    }

    std::string file_path;
    if(!runtime_config.emulate_hw){
        registers_fd = open(if_dict.get_control_bus().c_str(), O_RDWR | O_SYNC);
        if(registers_fd == -1){
            spdlog::error( "Error while mapping the axi control bus");
            exit(1);
        }

        fcore_fd = open(if_dict.get_cores_bus().c_str(), O_RDWR | O_SYNC);
        if(fcore_fd == -1){
            spdlog::error("Error while mapping the fcore rom bus");
            exit(1);
        }

        spdlog::info("Mapping {0} memory pages from the axi control bus, starting at address: 0x{1:x}", n_pages_ctrl, control_addr);
        registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_SHARED, registers_fd, control_addr);
        if(registers == MAP_FAILED) {
            spdlog::error("Cannot mmap control bus: {0}", strerror(errno));
            exit(1);
        }

        spdlog::info("Mapping {0} memory pages from the fcore programming bus, starting at address: ", n_pages_fcore, core_addr);
        fCore = (uint32_t*) mmap(nullptr, n_pages_fcore*4096, PROT_READ | PROT_WRITE, MAP_SHARED, fcore_fd, core_addr);
        if(fCore == MAP_FAILED) {
            spdlog::error("Cannot mmap fcore programming bus: {0}", strerror(errno));
            exit(1);
        }

    } else {
        registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(registers == MAP_FAILED) {
            spdlog::error("Cannot create /dev/mem emulator anonymous mapping: {0}", strerror(errno));
            exit(1);
        }
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

///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint64_t fpga_bridge::fcore_address_to_index(uint64_t address) const {
    if(core_addr>address){
        spdlog::critical("Tried to write the core address: 0x{0:x} which is below the minimum allowed: 0x{1:x}", address, control_addr);
        exit(-1);
    }
    return (address - core_addr) / 4;
}

///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint64_t fpga_bridge::register_address_to_index(uint64_t address) const {
    if(control_addr>address){
        spdlog::critical("Tried to write the control address: 0x{0:x} which is below the minimum allowed: 0x{1:x}", address, control_addr);
        exit(-1);
    }
    return (address - control_addr) / 4;
}

/// Load a program into the specified fCore instance through the AXI bus
/// \param address Address of the fCore instance
/// \param program Vector with the instructions of the program to load
/// \return #RESP_OK
responses::response_code fpga_bridge::apply_program(uint64_t address, std::vector<uint32_t> program) {
    spdlog::info("APPLY PROGRAM: address:  0x{0:x} program_size: {1}", address, program.size());
    uint32_t offset =  fcore_address_to_index(address);

    if(!runtime_config.emulate_hw) {
        for(int i = 0; i< program.size(); i++)
            fCore[i+offset] = program[i];
    }
    return responses::ok;
}

responses::response_code fpga_bridge::set_clock_frequency(std::vector<uint32_t> freq) {

    set_pl_clock(freq[0], freq[1]);
    return responses::ok;
}


responses::response_code fpga_bridge::apply_filter(uint64_t address, std::vector<uint32_t> taps) {
    spdlog::info("APPLY FILTER: address: 0x{0:x}  N. Filter Taps {1}", address, taps.size());
    auto filter_address= register_address_to_index(address);

    if(!runtime_config.emulate_hw) {
        for(int i = 0; i< taps.size(); i++){
            registers[filter_address] = taps[i];
            registers[filter_address+1] = i;
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

    registers[register_address_to_index(addr)] = buffer;
    return responses::ok;
}

void fpga_bridge::write_direct(uint64_t addr, uint32_t val) {
    spdlog::info("WRITE SINGLE REGISTER (DIRECT): addr 0x{0:x} value {1}", addr, val);
    registers[register_address_to_index(addr)] =val;
}

void fpga_bridge::write_proxied(uint64_t proxy_addr, uint32_t target_addr, uint32_t val) {
    spdlog::info("WRITE SINGLE REGISTER (AXIS PROXIED): proxy_addr 0x{0:x} addr 0x{1:x} value {2}",proxy_addr, target_addr, val);

    registers[register_address_to_index(proxy_addr+4)] = target_addr;
    registers[register_address_to_index(proxy_addr)] = val;
}

uint32_t fpga_bridge::read_direct(uint64_t address) {


    if(runtime_config.emulate_hw) {
        return rand() % 32767;
    } else{
        return registers[register_address_to_index(address)];
    }

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
