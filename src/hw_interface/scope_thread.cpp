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

#include "hw_interface/scope_thread.hpp"

thread_local volatile int fd_data; /// Scope driver file descriptor

/// Initializes the scope_handler infrastructure, opening the UIO driver file and writing to it to clear any outstanding
/// interrupts
/// \param driver_file Path of the driver file
/// \param buffer_size Size of the capture buffer
scope_thread::scope_thread() : data_gen(buffer_size){
    spdlog::trace("Scope handler emulate_control mode: {0}",runtime_config.emulate_hw);
    spdlog::info("Scope Handler initialization started");

    n_buffers_left = 0;
    internal_buffer_size = n_channels*buffer_size;
    sc_scope_data_buffer.reserve(internal_buffer_size);
    data_holding_buffer.reserve(n_channels*internal_buffer_size);
    scaling_factors = {1,1,1,1,1,1};
    channel_sizes = {16,16,16,16,16,16};
    channel_status = {
            {0,true},
            {1,true},
            {2,true},
            {3,true},
            {4,true},
            {5,true},
    };

    //mmap buffer
    fd_data = open(if_dict.get_data_bus().c_str(), O_RDWR| O_SYNC);
    if(fd_data == -1){
        std::cerr << std::strerror(errno);
    }
    dma_buffer = (uint64_t * ) malloc(n_channels*buffer_size*sizeof(uint64_t));

    spdlog::info("Scope handler initialization done");
}


responses::response_code scope_thread::start_capture(unsigned int n_buffers) {
    spdlog::info("START CAPTURE: n_buffers {0}", n_buffers);
    n_buffers_left = n_buffers;
    return responses::ok;
}

/// This function returns the number of data buffers left to capture
/// \return Number of buffers still to capture
unsigned int scope_thread::check_capture_progress() const {
    return n_buffers_left;
}

responses::response_code scope_thread::read_data(std::vector<nlohmann::json> &data_vector) {

    std::vector<std::vector<float>> data;

    spdlog::trace("READ_DATA: STARTING");
    read(fd_data, (void *) dma_buffer, internal_buffer_size * sizeof(uint64_t));
    spdlog::trace("READ_DATA: COPIED DATA FROM KERNEL");
    data = shunt_data(dma_buffer);
    spdlog::trace("READ_DATA: SHUNTING DONE");

    for(int i = 0; i<n_channels; i++){
        nlohmann::json ch_obj;
        if(channel_status[i]){
            ch_obj["channel"] = i;
            ch_obj["data"] = data[i];
            data_vector.push_back(ch_obj);
        }
    }
    return responses::ok;
}



std::vector<std::vector<float>> scope_thread::shunt_data(const volatile uint64_t * buffer_in) {
    std::vector<std::vector<float>> ret_data;
    for(int i = 0; i<n_channels; i++){
        ret_data.emplace_back();
    }
    for(int i = 0; i<internal_buffer_size; i++){

        auto raw_sample = buffer_in[i];
        int channel_base = GET_CHANNEL(raw_sample);
        auto sample = GET_DATA(raw_sample);
        auto metadata = channel_metadata(GET_METADATA(raw_sample));
        float data_sample;
        if(manual_metadata){
            data_sample = scale_data(sample, channel_sizes[channel_base], scaling_factors[channel_base], signed_status[channel_base], false);
        } else {
            data_sample = scale_data(sample, metadata.get_size(), scaling_factors[channel_base], metadata.is_signed(), metadata.is_float());
        }

        ret_data[channel_base].push_back(data_sample);
    }
    return ret_data;
}

float scope_thread::scale_data(uint32_t raw_sample, unsigned int size, float scaling_factor, bool is_signed, bool is_float) {

    float ret;

    int32_t sample;
    if(!is_signed) {
        sample = raw_sample & ((1 << size) - 1);
    } else {
        auto masked_sample = raw_sample & ((1<<size)-1);
        sample = sign_extend(masked_sample, size);
    }
    ret = scaling_factor*(float)sample;;
    if(is_float){
        memcpy(&ret, &raw_sample, sizeof(float));
    }
    return ret;
}

responses::response_code scope_thread::set_channel_widths(std::vector<uint32_t> &widths) {
    spdlog::info("SET_CHANNEL_WIDTHS: {0} {1} {2} {3} {4} {5}",widths[0], widths[1], widths[2], widths[3], widths[4], widths[5]);
    channel_sizes = widths;
    return responses::ok;
}

responses::response_code  scope_thread::set_scaling_factors(std::vector<float> &sf) {
    spdlog::info("SET_CHANNEL_WIDTHS: {0} {1} {2} {3} {4} {5}",sf[0], sf[1], sf[2], sf[3], sf[4], sf[5]);
    scaling_factors = sf;
    return responses::ok;
}

responses::response_code scope_thread::set_channel_status(std::unordered_map<int, bool> status) {
    channel_status = std::move(status);
    return responses::ok;
}

responses::response_code scope_thread::set_channel_signed(std::unordered_map<int, bool> ss) {
    spdlog::info("SET_CHANNEL_SIGNS: {0} {1} {2} {3} {4} {5}",ss[0], ss[1], ss[2], ss[3], ss[4], ss[5]);
    signed_status = std::move(ss);
    return responses::ok;
}

responses::response_code scope_thread::enable_manual_metadata() {
    spdlog::info("ENABLE MANUAL METADATA");
    manual_metadata = true;
    return responses::ok;
}

std::string scope_thread::get_acquisition_status() {
    spdlog::trace("GET_ACQUISITION_STATUS");
    return "";
}

responses::response_code scope_thread::set_acquisition(const acquisition_metadata &data) {
    spdlog::info("SET ACQUISITION: {} mode with {} trigger on channel {} at level {}",
                 data.mode,
                 data.trigger_mode,
                 data.trigger_source,
                 data.trigger_level
    );
    return responses::ok;
}

