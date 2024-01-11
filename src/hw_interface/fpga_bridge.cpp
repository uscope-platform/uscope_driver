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


void sigsegv_handler(int dummy) {
    std::cerr << "ERROR:A Segmentation fault happened while writing to an mmapped region" <<std::endl;
    exit(-1);
}

void sigbus_handler(int dummy) {
    std::cerr << "ERROR:A bus error happened while writing to an mmapped region" <<std::endl;
    exit(-1);
}


fpga_bridge::fpga_bridge(const std::string& driver_file, unsigned int dma_buffer_size, bool emulate_control, bool log, int log_level)
: scope_handler(driver_file, dma_buffer_size, emulate_control, log, log_level) {

    if(log){
        std::cout << "fpga_bridge initialization started"<< std::endl;
    }

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

    debug_mode = emulate_control;
    log_enabled = log;

    int n_pages_ctrl, n_pages_fcore;

    char const* t = getenv("MMAP_N_PAGES_CTRL");
    if(t == nullptr){
        n_pages_ctrl = 10;
    } else {
        n_pages_ctrl = std::stoi(std::string(t));
    }

    t = getenv("MMAP_N_PAGES_FCORE");
    if(t == nullptr){
        n_pages_fcore = 16;
    } else {
        n_pages_fcore = std::stoi(std::string(t));
    }

    std::string file_path;
    if(!emulate_control){
        registers_fd = open("/dev/uscope_BUS_0", O_RDWR | O_SYNC);
        if(registers_fd == -1){
            std::cerr << "error while mapping the axi control bus (M_GP0)" <<std::endl;
            exit(1);
        }

        fcore_fd = open("/dev/uscope_BUS_1", O_RDWR | O_SYNC);
        if(fcore_fd == -1){
            std::cerr << "error while mapping the fcore rom bus (M_GP1)" <<std::endl;
            exit(1);
        }

        std::cout << "Mapping " <<std::to_string(n_pages_ctrl)<<" memory pages from the axi control bus, starting at address: "<< control_addr <<std::endl;
        registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_SHARED, registers_fd, control_addr);
        if(registers == MAP_FAILED) {
            std::cerr << "Cannot mmap AXI GP0 bus: "<< strerror(errno) << std::endl;
            exit(1);
        }

        std::cout << "Mapping " <<std::to_string(n_pages_ctrl)<<" memory pages from the axi control bus, starting at address: "<< control_addr <<std::endl;
        fCore = (uint32_t*) mmap(nullptr, n_pages_fcore*4096, PROT_READ | PROT_WRITE, MAP_SHARED, fcore_fd, core_addr);
        if(fCore == MAP_FAILED) {
            std::cerr << "Cannot mmap AXI GP1 bus: "<< strerror(errno) << std::endl;
            exit(1);
        }

    } else {
        registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(registers == MAP_FAILED) {
            std::cerr << "Cannot create /dev/mem emulator anonymous mapping: "<< strerror(errno) << std::endl;
            exit(1);
        }
    }

    if(log){
        std::cout << "fpga_bridge initialization started"<< std::endl;
    }
}

/// This method loads a bitstrem by name through the linux kernel fpga_manager interface
/// \param bitstream Name of the bitstream to load
/// \return #RESP_OK if the file is found #RESP_ERR_BITSTREAM_NOT_FOUND otherwise
responses::response_code fpga_bridge::load_bitstream(const std::string& bitstream) {
    if(log_enabled) std::cout << "LOAD BITSTREAM: " << bitstream<<std::endl;

    if(!debug_mode){
        system("echo 0 > /sys/class/fpga_manager/fpga0/flags");

        std::string filename = "/lib/firmware/" + bitstream;
        //struct stat buffer;
        if(log_enabled) std::cout << filename << std::endl;

        if(std::filesystem::exists(filename)){
            std::string command = "echo " + bitstream + " > /sys/class/fpga_manager/fpga0/firmware";
            if(log_enabled) std::cout << command << std::endl;

            system(command.c_str());
            return responses::ok;
        } else {
            std::cerr << "ERROR: Bitstream not found" << filename << std::endl;
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
        if(log_enabled) std::cout << "WRITE SINGLE REGISTER (DIRECT) : addr "<< std::hex<< write_obj["address"] <<" value "<< std::dec<< write_obj["value"]<<std::endl;

        registers[register_address_to_index(write_obj["address"])] = write_obj["value"];
    } else if(write_obj["type"] == "proxied") {
         if(write_obj["proxy_type"] == "axis_constant"){
            if(log_enabled) std::cout << "WRITE SINGLE REGISTER (AXIS PROXIED) : proxy_addr "<< std::hex<< write_obj["proxy_address"] <<" addr "<< std::hex<< write_obj["address"] <<" value "<< std::dec<< write_obj["value"]<<std::endl;
            uint32_t proxy_addr = write_obj["proxy_address"];
            registers[register_address_to_index(proxy_addr+4)] = write_obj["address"];
            registers[register_address_to_index(proxy_addr)] = write_obj["value"];
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
    if(log_enabled) std::cout << "READ SINGLE REGISTER: addr "<< std::hex << address <<std::endl;
    if(debug_mode) {
        response_body["data"] = rand() % 32767;
    } else{
        response_body["data"] = registers[register_address_to_index(address)];
    }
    response_body["response_code"] = responses::as_integer(responses::ok);
    return response_body;
}

///  Start scope data capture
/// \param n_buffers Number of buffers to capture
/// \return #RESP_OK
responses::response_code fpga_bridge::start_capture(uint32_t n_buffers) {
    if(log_enabled) std::cout << "START CAPTURE: n_buffers "<< n_buffers<<std::endl;
    scope_handler.start_capture(n_buffers);
    return responses::ok;
}


/// Read scope data if ready
/// \param read_data pointer to the array the data will be put in
/// \return #RESP_OK if data is ready #RESP_DATA_NOT_READY otherwise
responses::response_code fpga_bridge::read_data(std::vector<nlohmann::json> &read_data) {
    if(debug_mode){
        if(log_enabled) std::cout << "READ DATA" << std::endl;
    }
    scope_handler.read_data(read_data);
    return responses::ok;
}

///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint64_t fpga_bridge::fcore_address_to_index(uint64_t address) const {
    return (address - core_addr) / 4;
}

///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint64_t fpga_bridge::register_address_to_index(uint64_t address) const {
    return (address - control_addr) / 4;
}

int fpga_bridge::check_capture_progress(unsigned int &progress) {
    progress = scope_handler.check_capture_progress();
    return responses::ok;
}

/// Set the channel widths for sign extension
/// \param widths list of channel widths
/// \return #RESP_OK
void fpga_bridge::stop_scope() {
    ioctl(fcore_fd,4);
    ioctl(registers_fd,4);
    scope_handler.stop_thread();
}

/// Load a program into the specified fCore instance through the AXI bus
/// \param address Address of the fCore instance
/// \param program Vector with the instructions of the program to load
/// \return #RESP_OK
responses::response_code fpga_bridge::apply_program(uint32_t address, std::vector<uint32_t> program) {
    std::cout<< "APPLY PROGRAM: address: " << std::hex << address << " program_size: "<< std::dec << program.size()<<std::endl;
    uint32_t offset = (address - core_addr)/4;

    if(!debug_mode) {
        for(int i = 0; i< program.size(); i++)
            fCore[i+offset] = program[i];
    }
    return responses::ok;
}

/// Set the channel widths for sign extension
/// \param widths list of channel widths
/// \return #RESP_OK
responses::response_code fpga_bridge::set_channel_widths( std::vector<uint32_t> widths) {
    if(log_enabled)
        std::cout << "SET_CHANNEL_WIDTHS:"<< std::to_string(widths[0]) << " " << std::to_string(widths[1]) << " " << std::to_string(widths[2]) << " " << std::to_string(widths[3]) << " " << std::to_string(widths[4]) << " " << std::to_string(widths[5]) <<std::endl;
    scope_handler.set_channel_widths(widths);
    return responses::ok;
}

responses::response_code fpga_bridge::set_scaling_factors(std::vector<float> sfs) {
    if(log_enabled){
        std::cout << "SET_SCALING_FACTORS: ";
        for(auto &s:sfs){
            std::cout << std::to_string(s) << " ";
        }
        std::cout << std::endl;
    }
    scope_handler.set_scaling_factors(sfs);
    return responses::ok;
}

responses::response_code fpga_bridge::set_clock_frequency(std::vector<uint32_t> freq) {
    if(log_enabled)
        std::cout << "SET_CLOCK FREQUENCY: clock #" + std::to_string(freq[0]) + " to " + std::to_string(freq[1]) + "Hz" <<std::endl;

    std::string command;

    if(arch=="zynq"){
        command = "echo " + std::to_string(freq[1]) + " > /sys/devices/soc0/fffc0000.uScope/fclk_" + std::to_string(freq[0]);
    } else{
        return responses::ok;
    }
    if(!debug_mode) {
        system(command.c_str());
    }
    return responses::ok;
}

responses::response_code fpga_bridge::set_channel_status(std::unordered_map<int, bool> channel_status) {
    scope_handler.set_channel_status(std::move(channel_status));
    return responses::ok;
}

responses::response_code fpga_bridge::set_channel_signed(std::unordered_map<int, bool> channel_signs) {
    if(log_enabled){
        std::cout << "SET_CHANNEL_SIGNS: ";
        for(auto &s:channel_signs){
            if(s.second){
                std::cout << "s ";
            } else {
                std::cout << "u ";
            }

        }
        std::cout << std::endl;
    }
    scope_handler.set_channel_signed(std::move(channel_signs));
    return responses::ok;
}

responses::response_code fpga_bridge::apply_filter(uint32_t address, std::vector<uint32_t> taps) {
    std::cout<< "APPLY FILTER: address: " << std::hex << address << " N. Filter Taps "<< std::dec << taps.size()<<std::endl;

    auto filter_address= register_address_to_index(address);

    if(!debug_mode) {
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


