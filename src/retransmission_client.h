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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

#include "comm.h"
#include "common/log.h"
#include "endpoint.h"

class RetransmissionClient {
public:
    RetransmissionClient(const std::string &name, uint32_t sender_id);
    ~RetransmissionClient();

    int start();
    void stop();
    void handle_read();
    int write_msg(Endpoint* endpoint_entry, const struct buffer *buf);

private:
    int _fd{-1};
    uint32_t _sender_id;
    struct sockaddr_in _addr {};

    std::thread _io_thread;
    std::atomic<bool> _running{false};
    std::queue<std::vector<uint8_t>> _send_queue;
    std::mutex _send_queue_mtx;
    int _signal_fd{-1};
    int _epoll_fd{-1};

    void _io_loop_thread();
    int _connect();
    void _disconnect();
    void _send_queued_messages();
};
