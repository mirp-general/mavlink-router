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

#include "channellogger.h"
#include "channel_stats_packet.h"
#include "mainloop.h"
#include <vector>
#include <chrono>
#include <functional>

ChannelLogger::ChannelLogger(std::vector<std::shared_ptr<Endpoint>> &endpoints,
                             std::shared_ptr<RetransmissionClient> retransmission_client,
                             uint32_t sender_id)
    : _endpoints(endpoints)
    , _retransmission_client(std::move(retransmission_client))
    , _sender_id(sender_id)
{}

ChannelLogger::~ChannelLogger() {
    stop_logging();
}

void ChannelLogger::start_logging(long long interval_ms) {
    if (_log_timeout) {
        return; // Already logging
    }
    _log_timeout = Mainloop::get_instance().add_timeout(
        interval_ms,
        std::bind(&ChannelLogger::_log_timeout_cb, this, std::placeholders::_1),
        this);
}

void ChannelLogger::stop_logging() {
    if (_log_timeout) {
        Mainloop::get_instance().del_timeout(_log_timeout);
        _log_timeout = nullptr;
    }
}

bool ChannelLogger::_log_timeout_cb(void *data) {
    auto *self = static_cast<ChannelLogger *>(data);
    self->_send_channel_stats();
    return true; // Continue logging periodically
}

void ChannelLogger::_send_channel_stats() {
    long long time_interval_ms = 1000; // Assuming 1-second interval for bitrate calculation

    for (const auto &e : _endpoints) {
        if (e->get_metrics() == nullptr) {
            continue;
        }

        ChannelStatsPacket packet{
            _sender_id,
            0x02, // type_id for stats
            e->get_type(),
            e->get_name(),
            e->get_group_name(),
            e->get_metrics()->is_active(),
            e->get_metrics()->get_incoming_bitrate_kbps(time_interval_ms),
            e->get_metrics()->get_outgoing_bitrate_kbps(time_interval_ms),
            e->get_metrics()->get_incoming_jitter_ms()
        };

        std::vector<uint8_t> serialized_packet = packet.Serialize();
        if (_retransmission_client) {
            _retransmission_client->write_stats_msg(e.get(), serialized_packet); // New method to be implemented
        }
        e->get_metrics()->reset_prev_bytes();
    }
}
