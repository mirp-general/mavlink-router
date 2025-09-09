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
#include <memory>
#include <vector>

#include "endpoint.h"
#include "retransmission_client.h"
#include "timeout.h"

class ChannelLogger {
public:
    ChannelLogger(std::vector<std::shared_ptr<Endpoint>> &endpoints, 
                  std::shared_ptr<RetransmissionClient> retransmission_client, 
                  uint32_t sender_id);
    ~ChannelLogger();

    void start_logging(long long interval_ms = 1000);
    void stop_logging();

private:
    std::vector<std::shared_ptr<Endpoint>> &_endpoints;
    std::shared_ptr<RetransmissionClient> _retransmission_client;
    uint32_t _sender_id;
    Timeout *_log_timeout = nullptr;

    bool _log_timeout_cb(void *data);
    void _send_channel_stats();
};
