//   Copyright 2026 Filippo Savi <filssavi@gmail.com>
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

#include "hw_interface/rpu/rpu_manager.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fcntl.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

#include "hw_interface/operations/interfaces_dictionary.hpp"

namespace {
    constexpr std::array<uint8_t, 4> elf_magic = {0x7f, 'E', 'L', 'F'};

    /// name of a core's staged firmware, relative to the firmware directory
    std::string firmware_name(uint8_t core) {
        return "rpu_" + std::to_string(core) + ".elf";
    }

    /// firmware name as remoteproc wants it: relative to the kernel firmware search path (/lib/firmware)
    std::string remoteproc_firmware_name(const std::string &dir, uint8_t core) {
        return std::filesystem::path(dir).filename().string() + "/" + firmware_name(core);
    }
}

fcore::result<void> rpu_manager::write_file(const std::string &path, const std::string &content) {
    const int fd = open(path.c_str(), O_WRONLY);
    if(fd < 0) {
        return fcore::fail(fcore::error_code::io_error, "Failed to open " + path + ": " + std::strerror(errno));
    }
    const auto written = write(fd, content.data(), content.size());
    const int write_errno = errno;
    close(fd);
    if(written < 0) {
        return fcore::fail(fcore::error_code::io_error, "Failed to write '" + content + "' to " + path + ": " + std::strerror(write_errno));
    }
    return {};
}

fcore::result<std::string> rpu_manager::read_file(const std::string &path) {
    std::ifstream ifs(path);
    if(!ifs) {
        return fcore::fail(fcore::error_code::io_error, "Failed to open " + path + ": " + std::strerror(errno));
    }
    std::string value;
    ifs >> value;
    return value;
}

fcore::result<std::string> rpu_manager::get_state(uint8_t core) {
    auto remoteproc = if_dict.get_remoteproc_if(core);
    if(!remoteproc) return fcore::fail(std::move(remoteproc).error());
    return read_file(remoteproc.value() + "/state");
}

fcore::result<void> rpu_manager::stop(uint8_t core) {
    auto remoteproc = if_dict.get_remoteproc_if(core);
    if(!remoteproc) return fcore::fail(std::move(remoteproc).error());
    const auto state_file = remoteproc.value() + "/state";

    auto state = read_file(state_file);
    if(!state) return fcore::fail(std::move(state).error());
    if(state.value() == "offline") return {};

    // running, crashed, attached...: whatever it is doing, the core has to go offline
    spdlog::info("RPU: stopping core {0} (state {1})", core, state.value());
    if(auto stopped = write_file(state_file, "stop"); !stopped) {
        return fcore::fail(fcore::error_code::io_error, "Unable to stop RPU core " + std::to_string(core) +
            " (state " + state.value() + "): " + stopped.error().what());
    }
    state = read_file(state_file);
    if(!state) return fcore::fail(std::move(state).error());
    if(state.value() != "offline") {
        return fcore::fail(fcore::error_code::io_error, "RPU core " + std::to_string(core) + " did not stop, its state is " + state.value());
    }
    return {};
}

fcore::result<void> rpu_manager::start_core(uint8_t core, const std::string &remoteproc) {
    spdlog::info("RPU: starting core {0}", core);
    if(auto started = write_file(remoteproc + "/state", "start"); !started) {
        return fcore::fail(fcore::error_code::io_error, "Unable to start RPU core " + std::to_string(core) + ": " + started.error().what());
    }
    auto state = read_file(remoteproc + "/state");
    if(!state) return fcore::fail(std::move(state).error());
    if(state.value() != "running") {
        return fcore::fail(fcore::error_code::io_error, "RPU core " + std::to_string(core) + " did not start, its state is " + state.value());
    }
    return {};
}

fcore::result<void> rpu_manager::load_program(uint8_t core, const std::vector<uint8_t> &program) {
    auto remoteproc = if_dict.get_remoteproc_if(core);
    if(!remoteproc) return fcore::fail(std::move(remoteproc).error());
    auto dir = if_dict.get_rpu_firmware_dir();
    if(!dir) return fcore::fail(std::move(dir).error());

    if(program.size() < elf_magic.size() || !std::equal(elf_magic.begin(), elf_magic.end(), program.begin())) {
        return fcore::fail(fcore::error_code::invalid_argument, "The program for RPU core " + std::to_string(core) + " is not an ELF file");
    }

    // a core that is not offline is stopped and reloaded: remoteproc only takes a new firmware while offline
    if(auto stopped = stop(core); !stopped) return stopped;

    spdlog::info("RPU: loading a {0} bytes program on core {1}", program.size(), core);
    // the staging directory is not part of a stock image, so it is made on first use
    std::error_code dir_ec;
    std::filesystem::create_directories(dir.value(), dir_ec);
    if(dir_ec) {
        return fcore::fail(fcore::error_code::io_error, "Failed to create the firmware directory " + dir.value() + ": " + dir_ec.message());
    }

    const auto staged = std::filesystem::path(dir.value()) / firmware_name(core);
    {
        std::ofstream ofs(staged, std::ios::binary | std::ios::trunc);
        if(!ofs) {
            return fcore::fail(fcore::error_code::io_error, "Failed to create the firmware file " + staged.string() + ": " + std::strerror(errno));
        }
        ofs.write(reinterpret_cast<const char *>(program.data()), static_cast<std::streamsize>(program.size()));
        if(!ofs.flush()) {
            std::error_code ec;
            std::filesystem::remove(staged, ec);
            return fcore::fail(fcore::error_code::io_error, "Failed to write the firmware file " + staged.string());
        }
    }

    // remoteproc reads the file when the core starts, so it can go as soon as the start is done (or failed)
    auto result = write_file(remoteproc.value() + "/firmware", remoteproc_firmware_name(dir.value(), core));
    if(result) result = start_core(core, remoteproc.value());

    std::error_code ec;
    std::filesystem::remove(staged, ec);
    if(ec) spdlog::warn("RPU: unable to remove the firmware file {0}: {1}", staged.string(), ec.message());

    return result;
}
