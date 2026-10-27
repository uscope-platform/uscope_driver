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


#include "hw_interface/bus/bus_accessor.hpp"

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string_view>

#include <fmt/format.h>

std::mutex m;

std::atomic<bool> bus_accessor::sink_mode{false};


void sigsegv_handler(int dummy) {
    spdlog::error("Segmentation fault encounteded while communicating with FPGA");
    exit(-1);
}

void sigbus_handler(int dummy) {
    spdlog::error("Bus error encountered while communicating with FPGA");
    exit(-1);
}

namespace {
    /// Number of pages to map, from the environment variable if it is set
    fcore::result<int> pages_from_environment(const char *name) {
        constexpr int default_pages = 344000;
        const char *value = std::getenv(name);
        if(value == nullptr) return default_pages;

        const std::string_view text(value);
        int pages = 0;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), pages);
        if(ec != std::errc() || end != text.data() + text.size() || pages <= 0){
            return fcore::fail(fcore::error_code::invalid_argument, std::string(name) + " must be a positive integer, got '" + value + "'");
        }
        return pages;
    }
}

fcore::result<std::shared_ptr<bus_accessor>> bus_accessor::create() {
    spdlog::info("fpga_bridge initialization started");

    signal(SIGSEGV,sigsegv_handler);
    signal(SIGBUS,sigbus_handler);

    auto accessor = std::make_shared<bus_accessor>(construction_key{});
    if(auto mapped = accessor->map_buses(); !mapped) return fcore::fail(std::move(mapped).error());

    spdlog::info("fpga_bridge initialization done");
    return accessor;
}

fcore::result<void> bus_accessor::map_buses() {
    std::string arch;
    if (const char * env_val = std::getenv("ARCH")) {
        arch = env_val;
    } else {
        return fcore::fail(fcore::error_code::invalid_argument, "The architecture must be selected with the ARCH environment variable");
    }
    if(arch == "zynqmp"){
        control_addr = ZYNQMP_REGISTERS_BASE_ADDR;
        core_addr = ZYNQMP_FCORE_BASE_ADDR;
    } else {
        control_addr = ZYNQ_REGISTERS_BASE_ADDR;
        core_addr = ZYNQ_FCORE_BASE_ADDR;
    }

    auto n_pages_ctrl = pages_from_environment("MMAP_N_PAGES_CTRL");
    if(!n_pages_ctrl) return fcore::fail(std::move(n_pages_ctrl).error());
    auto n_pages_fcore = pages_from_environment("MMAP_N_PAGES_FCORE");
    if(!n_pages_fcore) return fcore::fail(std::move(n_pages_fcore).error());

    auto control_bus = if_dict.get_control_bus();
    if(!control_bus) return fcore::fail(std::move(control_bus).error());
    auto cores_bus = if_dict.get_cores_bus();
    if(!cores_bus) return fcore::fail(std::move(cores_bus).error());

    auto registers_fd = open(control_bus.value().c_str(), O_RDWR | O_SYNC);
    if(registers_fd == -1){
        return fcore::fail(fcore::error_code::io_error, "Error while mapping the axi control bus " + control_bus.value() + ": " + strerror(errno));
    }

    auto fcore_fd = open(cores_bus.value().c_str(), O_RDWR | O_SYNC);
    if(fcore_fd == -1){
        return fcore::fail(fcore::error_code::io_error, "Error while mapping the fcore rom bus " + cores_bus.value() + ": " + strerror(errno));
    }

    spdlog::info("Mapping {0} memory pages from the axi control bus, starting at address: 0x{1:x}", n_pages_ctrl.value(), control_addr);
    control_words = static_cast<uint64_t>(n_pages_ctrl.value())*4096/4;
    registers = (uint32_t*) mmap(nullptr, n_pages_ctrl.value()*4096, PROT_READ | PROT_WRITE, MAP_SHARED, registers_fd, control_addr);
    if(registers == MAP_FAILED) {
        return fcore::fail(fcore::error_code::io_error, std::string("Cannot mmap control bus: ") + strerror(errno));
    }

    spdlog::info("Mapping {0} memory pages from the fcore programming bus, starting at address: 0x{1:x}", n_pages_fcore.value(), core_addr);
    fCore = (uint32_t*) mmap(nullptr, n_pages_fcore.value()*4096, PROT_READ | PROT_WRITE, MAP_SHARED, fcore_fd, core_addr);
    fcore_words = static_cast<uint64_t>(n_pages_fcore.value())*4096/4;
    if(fCore == MAP_FAILED) {
        return fcore::fail(fcore::error_code::io_error, std::string("Cannot mmap fcore programming bus: ") + strerror(errno));
    }
    return {};
}

fcore::result<void> bus_accessor::write_register(const std::vector<uint64_t>& addresses, uint64_t data) {
    if(!sink_mode){
        // all the addresses are validated before touching anything, so a rejected write has no effect at all
        if(addresses.size() ==1){
            auto index = register_address_to_index(addresses[0]);
            if(!index) return fcore::fail(std::move(index).error());
            std::lock_guard lock(m);
            registers[index.value()] = data;
        } else {
            auto address_index = register_address_to_index(addresses[1]+4);
            if(!address_index) return fcore::fail(std::move(address_index).error());
            auto data_index = register_address_to_index(addresses[1]);
            if(!data_index) return fcore::fail(std::move(data_index).error());
            std::lock_guard lock(m);
            registers[address_index.value()] = addresses[0];
            registers[data_index.value()] = data;
        }
    }
    operations.push_back({addresses, {data}, control_plane_write});
    return {};
}

fcore::result<uint32_t> bus_accessor::read_register(const std::vector<uint64_t>& address) {
    if(!sink_mode){
        uint32_t ret_val = 0;
        if(address.size()==1){
            auto reg_n = register_address_to_index(address[0]);
            if(!reg_n) return fcore::fail(std::move(reg_n).error());
            spdlog::trace("READ from Register #{0} at address {1}", reg_n.value(), address[0]);
            std::lock_guard lock(m);
            ret_val = registers[reg_n.value()];
        }
        operations.push_back({address, {0}, control_plane_read});
        return ret_val;
    } else {
        operations.push_back({address, {0}, control_plane_read});
        return rand()%100;
    }
}

fcore::result<uint64_t> bus_accessor::fcore_address_to_index(uint64_t address) const {
    if(address < core_addr || (address - core_addr) / 4 >= fcore_words){
        return fcore::fail(fcore::error_code::invalid_argument,"Core bus address 0x" + fmt::format("{:x}", address) + " is outside of the mapped window (from 0x"
                                + fmt::format("{:x}", core_addr) + " for " + std::to_string(fcore_words*4) + " bytes)");
    }
    return (address - core_addr) / 4;
}

std::pair<std::string, std::string> bus_accessor::get_hardware_simulation_data() {
    std::string rom_plane, control_plane;
    auto ops = get_operations();
    for(auto &o:ops) {
        if(o.type == rom_plane_write) {
            auto rom_address = o.address[0];
            for(int i = 0; i<o.data.size(); i++) {
                rom_plane += std::to_string(rom_address + 4*i) + ":" + std::to_string(o.data[i]) + '\n';
            }
        } else if(o.type == control_plane_write) {
            for(int i = 0; i<o.data.size(); i++) {
                control_plane += std::to_string(o.address[i]) + ":"  + std::to_string(o.data[i]) + "\n";
            }
        }
    }
    return {rom_plane, control_plane};
}

fcore::result<uint64_t> bus_accessor::register_address_to_index(uint64_t address) const {
    if(address < control_addr || (address - control_addr) / 4 >= control_words){
        return fcore::fail(fcore::error_code::invalid_argument,"Control bus address 0x" + fmt::format("{:x}", address) + " is outside of the mapped window (from 0x"
                                + fmt::format("{:x}", control_addr) + " for " + std::to_string(control_words*4) + " bytes)");
    }
    return (address - control_addr) / 4;
}

fcore::result<void> bus_accessor::load_program(uint64_t address, const std::vector<uint32_t> program) {

    std::vector<uint64_t> pn;
    std::vector<uint64_t> a = {address};

    if(sink_mode){
        for (auto &p:program) {
            pn.push_back(p);
        }
    } else {
        // an address outside of the mapped window would be a segmentation fault, which kills the driver
        if(address < core_addr || (address-core_addr)/4 + program.size() > fcore_words){
            return fcore::fail(fcore::error_code::invalid_argument, "Cannot load the program at address 0x" + fmt::format("{:x}", address) +
                                     ": the programming bus is mapped from 0x" + fmt::format("{:x}", core_addr) +
                                     " for " + std::to_string(fcore_words*4) + " bytes (check the ARCH of the driver and the code address of the core)");
        }
        auto base_index = fcore_address_to_index(address);
        if(!base_index) return fcore::fail(std::move(base_index).error());
        std::lock_guard lock(m);
        a = {address};
        for(int i = 0; i< program.size(); i++){
            fCore[i+base_index.value()] = program[i];
            pn.push_back(program[i]);
            usleep(1);
        }
    }
    operations.push_back({a, pn, rom_plane_write});
    return {};
}

