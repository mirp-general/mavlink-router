/*
 * This file is part of the MAVLink Router project
 *
 * Copyright (C) 2024  Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "endpoint_metrics.h"
#include <chrono>
#include <numeric>
#include <cmath>

EndpointMetrics::EndpointMetrics() : 
    _last_read_time(std::chrono::steady_clock::now()),
    _last_write_time(std::chrono::steady_clock::now()),
    _prev_packet_read_time(std::chrono::steady_clock::now()) {}

void EndpointMetrics::update_read_metrics(size_t bytes_read, const std::chrono::steady_clock::time_point& current_time) {
    _last_read_time = current_time;
    _current_read_bytes += bytes_read;

    if (_prev_packet_read_time != std::chrono::steady_clock::time_point()) {
        long long inter_arrival_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - _prev_packet_read_time).count();
        _inter_arrival_times.push_back(inter_arrival_time);
        if (_inter_arrival_times.size() > JITTER_BUFFER_SIZE) {
            _inter_arrival_times.pop_front();
        }
    }
    _prev_packet_read_time = current_time;
}

void EndpointMetrics::update_write_metrics(size_t bytes_written, const std::chrono::steady_clock::time_point& current_time) {
    _last_write_time = current_time;
    _current_write_bytes += bytes_written;
}

bool EndpointMetrics::is_active(long long timeout_ms) const {
    auto now = std::chrono::steady_clock::now();
    auto read_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_read_time).count();
    auto write_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_write_time).count();

    return read_diff < timeout_ms || write_diff < timeout_ms;
}

float EndpointMetrics::get_incoming_bitrate_kbps(long long time_interval_ms) const {
    if (time_interval_ms == 0) return 0.0f;
    uint64_t bytes_read_since_last_check = _current_read_bytes - _prev_read_bytes;
    return (float)bytes_read_since_last_check / time_interval_ms * 1000.0f / 1024.0f;
}

float EndpointMetrics::get_outgoing_bitrate_kbps(long long time_interval_ms) const {
    if (time_interval_ms == 0) return 0.0f;
    uint64_t bytes_written_since_last_check = _current_write_bytes - _prev_write_bytes;
    return (float)bytes_written_since_last_check / time_interval_ms * 1000.0f / 1024.0f;
}

float EndpointMetrics::get_incoming_jitter_ms() const {
    if (_inter_arrival_times.size() < 2) {
        return 0.0f;
    }

    double sum = 0.0;
    for (long long time : _inter_arrival_times) {
        sum += time;
    }
    double mean = sum / _inter_arrival_times.size();

    double sq_diff_sum = 0.0;
    for (long long time : _inter_arrival_times) {
        sq_diff_sum += (time - mean) * (time - mean);
    }
    double variance = sq_diff_sum / _inter_arrival_times.size();
    return static_cast<float>(std::sqrt(variance));
}

void EndpointMetrics::reset_prev_bytes() {
    _prev_read_bytes = _current_read_bytes;
    _prev_write_bytes = _current_write_bytes;
}
