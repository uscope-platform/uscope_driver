// Copyright 2024 Filippo Savi
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

#ifndef USCOPE_DRIVER_INTERFACES_DICTIONARY_HPP
#define USCOPE_DRIVER_INTERFACES_DICTIONARY_HPP

#include <utility>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "errors/result.hpp"

class interfaces_dictionary {
public:
    /// Selects the architecture, which has to be one of the known ones
    fcore::result<void> set_arch(const std::string &a) {
        if(!is_valid_arch(a)){
            return fcore::fail(fcore::error_code::invalid_argument, "Unknown architecture '" + a + "', the valid ones are: " + valid_archs());
        }
        arch = a;
        return {};
    };
    bool is_valid_arch(const std::string &a) const {
        return control_bus_if.contains(a);
    };
    std::string valid_archs() const {
        std::string ret;
        for(auto &[name, _]: control_bus_if) ret += (ret.empty() ? "" : ", ") + name;
        return ret;
    };
    fcore::result<std::string> get_control_bus() const {
        return lookup(control_bus_if, "configuration bus");
    };
    fcore::result<std::string> get_cores_bus() const{
        return lookup(cores_bus_if, "cores bus");
    };

    fcore::result<std::string> get_data_bus() const {
        return lookup(data_bus_if, "data bus");
    };

    fcore::result<std::string> get_rpu_clock_if() const {
        if (arch == "zynq" )
            return fcore::fail(fcore::error_code::invalid_argument, "The zynq 7000 series of processors does not have an RPU");
        return lookup(rpu_clock_if, "configuration");
    };

    /// remoteproc directory of an RPU core (core N is remoteprocN), holding the firmware and state files
    fcore::result<std::string> get_remoteproc_if(uint8_t core) const {
        if (arch == "zynq")
            return fcore::fail(fcore::error_code::invalid_argument, "The zynq 7000 series of processors does not have an RPU");
        if(core > 1) return fcore::fail(fcore::error_code::invalid_argument, "The RPU has only cores 0 and 1, requested core #" + std::to_string(core));
        auto base = lookup(remoteproc_if, "remoteproc interface");
        if(!base) return base;
        return base.value() + std::to_string(core);
    };

    /// Directory the RPU firmware is staged in, it must be inside the kernel firmware search path
    fcore::result<std::string> get_rpu_firmware_dir() const {
        if (arch == "zynq")
            return fcore::fail(fcore::error_code::invalid_argument, "The zynq 7000 series of processors does not have an RPU");
        return lookup(rpu_firmware_dir, "rpu firmware directory");
    };

    fcore::result<std::string> get_clock_if(uint8_t clock_number) const {
        if(clock_number >3) return fcore::fail(fcore::error_code::invalid_argument, "The PS<->PL interface has only clocks 0 to 3, requested variation of clock #" + std::to_string(clock_number));
        auto base = lookup(clock_if, "configuration");
        if(!base) return base;
        return base.value() + std::to_string(clock_number);
    };


    fcore::result<std::string> get_buffer_address_if() const {
        return lookup(buffer_address_if, "configuration");
    };

    fcore::result<std::string> get_fpga_flags_if() const {
        return lookup(fpga_manager_flags_if, "the fpga manager flags");
    };

    fcore::result<std::string> get_fpga_bitstream_if() const {
        return lookup(fpga_manager_bitstream_if, "the fpga manager bitstream");
    };

    fcore::result<std::string> get_fpga_state_if() const {
        return lookup(fpga_manager_state_if, "the fpga manager state");
    };



private:

    std::unordered_map<std::string, std::string> fpga_manager_state_if = {
            {"zynq", "/dev/uscope_bitstream"},
            {"zynqmp", "/dev/uscope_bitstream"},
            {"testing", "/tmp/fpga_manager_state"}
    };

    std::unordered_map<std::string, std::string> fpga_manager_bitstream_if = {
            {"zynq", "/dev/uscope_bitstream"},
            {"zynqmp", "/dev/uscope_bitstream"},
            {"testing", "/tmp/fpga_bitstream_if"}
    };

    std::unordered_map<std::string, std::string> fpga_manager_flags_if = {
            {"zynq", "/sys/class/fpga_manager/fpga0/flags"},
            {"zynqmp", "/sys/class/fpga_manager/fpga0/flags"},
            {"testing", "/tmp/fpga_flags_if"}
    };

    std::unordered_map<std::string, std::string> control_bus_if = {
            {"zynq", "/dev/uscope_BUS_0"},
            {"zynqmp", "/dev/uscope_BUS_0"},
            {"testing", "/tmp/control_bus_if"}
    };
    std::unordered_map<std::string, std::string> cores_bus_if = {
            {"zynq", "/dev/uscope_BUS_1"},
            {"zynqmp", "/dev/uscope_BUS_1"},
            {"testing", "/tmp/cores_bus_if"}
    };
    std::unordered_map<std::string, std::string> data_bus_if = {
            {"zynq", "/dev/uscope_data"},
            {"zynqmp", "/dev/uscope_data"},
            {"testing", "/tmp/data_bus_if"}
    };
    std::unordered_map<std::string, std::string> clock_if = {
            {"zynq", "/sys/devices/soc0/fffc0000.uScope/fclk_"},
            {"zynqmp", "/sys/devices/platform/fffc000000008000.uScope/fclk_"},
            {"testing", "/tmp/clock_if_"}
    };

    std::unordered_map<std::string, std::string> rpu_clock_if = {
        {"zynq", ""},
        {"zynqmp", "/sys/devices/platform/fffc000000008000.uScope/rpu_clk"},
        {"testing", "/tmp/rpu_clock_if_"}
    };
    std::unordered_map<std::string, std::string> remoteproc_if = {
        {"zynq", ""},
        {"zynqmp", "/sys/class/remoteproc/remoteproc"},
        {"testing", "/tmp/remoteproc"}
    };
    /// the firmware names written to remoteproc are relative to the firmware search path (/lib/firmware)
    std::unordered_map<std::string, std::string> rpu_firmware_dir = {
        {"zynq", ""},
        {"zynqmp", "/lib/firmware/uscope"},
        {"testing", "/tmp/firmware/uscope"}
    };
    std::unordered_map<std::string, std::string> buffer_address_if = {
            {"zynq", "/sys/devices/soc0/fffc0000.uScope/dma_addr"},
            {"zynqmp", "/sys/devices/platform/fffc000000008000.uScope/dma_addr"},
            {"testing", "/tmp/buffer_address_if"}
    };
    std::string arch = "trap";

    fcore::result<std::string> lookup(const std::unordered_map<std::string, std::string> &interfaces, const std::string &what) const {
        if(arch == "trap") return fcore::fail(fcore::error_code::invalid_argument, "Attempted access to " + what + " before architecture selection");
        auto it = interfaces.find(arch);
        if(it == interfaces.end()) return fcore::fail(fcore::error_code::invalid_argument, "Unknown architecture '" + arch + "'");
        return it->second;
    }

};

extern interfaces_dictionary if_dict;


#endif //USCOPE_DRIVER_INTERFACES_DICTIONARY_HPP
