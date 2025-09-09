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

#pragma once

#include <chrono>
#include <deque>
#include <numeric>
#include <cmath>

class EndpointMetrics {
public:
    EndpointMetrics();

    void update_read_metrics(size_t bytes_read, const std::chrono::steady_clock::time_point& current_time);
    void update_write_metrics(size_t bytes_written, const std::chrono::steady_clock::time_point& current_time);

    bool is_active(long long timeout_ms = 5000) const;
    float get_incoming_bitrate_kbps(long long time_interval_ms) const;
    float get_outgoing_bitrate_kbps(long long time_interval_ms) const;
    float get_incoming_jitter_ms() const;

    void reset_prev_bytes();

private:
    std::chrono::steady_clock::time_point _last_read_time;
    std::chrono::steady_clock::time_point _last_write_time;
    uint64_t _current_read_bytes = 0;
    uint64_t _current_write_bytes = 0;
    uint64_t _prev_read_bytes = 0;
    uint64_t _prev_write_bytes = 0;
    std::deque<long long> _inter_arrival_times; // For jitter calculation, store last N inter-arrival times
    std::chrono::steady_clock::time_point _prev_packet_read_time;
    static constexpr size_t JITTER_BUFFER_SIZE = 100; // Store last 100 inter-arrival times
};
