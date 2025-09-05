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

#include "retransmission_client.h"

#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/uio.h>
#include <unistd.h>

#include <vector>

#include "endpoint.h"
#include "packet_log.h"

#define RETRANSMISSION_CLIENT_PORT 12345

RetransmissionClient::RetransmissionClient(const std::string &name,
                                           uint32_t sender_id)
    : _sender_id{sender_id}, _running{false}, _signal_fd{-1}, _epoll_fd{-1} {
  _addr.sin_family = AF_INET;
  _addr.sin_port = htons(RETRANSMISSION_CLIENT_PORT);
  _addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Connect to localhost

  _epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (_epoll_fd < 0) {
    log_error("RetransmissionClient: Failed to create epoll instance: %m");
    return;
  }

  _signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (_signal_fd < 0) {
    log_error("RetransmissionClient: Failed to create eventfd: %m");
    close(_epoll_fd);
    _epoll_fd = -1;
    return;
  }

  struct epoll_event epev = {};
  epev.events = EPOLLIN;
  epev.data.fd = _signal_fd;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _signal_fd, &epev) < 0) {
    log_error("RetransmissionClient: Failed to add signal_fd to epoll: %m");
    close(_signal_fd);
    close(_epoll_fd);
    _signal_fd = -1;
    _epoll_fd = -1;
    return;
  }
}

RetransmissionClient::~RetransmissionClient() {
  stop();
  if (_signal_fd >= 0) {
    close(_signal_fd);
  }
  if (_epoll_fd >= 0) {
    close(_epoll_fd);
  }
  // Free any remaining buffers in the queue
  while (!_send_queue.empty()) {
    // delete _send_queue.front();
    _send_queue.pop();
  }
}

int RetransmissionClient::start() {
  if (_running.load(std::memory_order_acquire)) {
    return 0;  // Already running
  }

  _running.store(true, std::memory_order_release);
  _io_thread = std::thread(&RetransmissionClient::_io_loop_thread, this);

  // Initial connection attempt
  if (_fd < 0) {
    _connect();
  }
  return 0;
}

void RetransmissionClient::stop() {
  if (!_running.load(std::memory_order_acquire)) {
    return;  // Not running
  }

  _running.store(false, std::memory_order_release);

  // Signal the IO thread to wake up and exit
  uint64_t val = 1;
  if (write(_signal_fd, &val, sizeof(val)) < 0) {
    log_error("RetransmissionClient: Failed to write to signal_fd: %m");
  }

  if (_io_thread.joinable()) {
    _io_thread.join();
  }
  _disconnect();  // Disconnect and cleanup socket
}

void RetransmissionClient::handle_read() {
  // Not expecting to read anything from the retransmission server
  // If data is received, it will be discarded for now.
  char buf[256];
  ssize_t n = ::read(_fd, buf, sizeof(buf));
  if (n < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      log_error("RetransmissionClient: Error reading from socket: %m");
      _disconnect();
    }
  } else if (n == 0) {
    log_info(
        "RetransmissionClient: Server closed connection. Disconnecting...");
    _disconnect();
  }
}

int RetransmissionClient::write_msg(Endpoint *endpoint_entry,
                                    const struct buffer *buf) {
  // Copy the buffer data to be owned by the queue
  //   std::vector<uint8_t> new_buf();

  PacketLog packet_log{
      this->_sender_id,
      0x01,  // packet log,
      buf->curr.msg_id,
      buf->curr.target_sysid,
      buf->curr.target_compid,
      buf->curr.src_sysid,
      buf->curr.src_compid,
      endpoint_entry->get_type(),
      endpoint_entry->get_name(),
      endpoint_entry->get_group_name(),
      std::vector<uint8_t>(buf->curr.payload,
                           buf->curr.payload + buf->curr.payload_len)
  };
  std::vector<uint8_t> new_buf = packet_log.Serialize();
  //   const uint32_t sender_id = this->_sender_id;
  //   const uint8_t type_id = 0x01;  // packet log
  //   const uint8_t msg_id = buf->curr.msg_id;
  //   const uint8_t target_sysid = buf->curr.target_sysid;
  //   const uint8_t target_compid = buf->curr.target_compid;
  //   const uint8_t src_sysid = buf->curr.src_sysid;
  //   const uint8_t src_compid = buf->curr.src_compid;

  //   const std::string e_type = endpoint_entry->get_type();
  //   const std::string e_name = endpoint_entry->get_name();
  //   const std::string e_group_name = endpoint_entry->get_group_name();

  //   const uint8_t payload_len = buf->curr.payload_len;
  //   const uint8_t *payload = buf->curr.payload;

  // new_buf[]
  // struct buffer *new_buf = (struct buffer *)malloc(sizeof(struct buffer) +
  // buf->len); if (!new_buf) {
  //     log_error("RetransmissionClient: Failed to allocate buffer for
  //     message."); return -ENOMEM;
  // }
  // memcpy(new_buf, buf, sizeof(struct buffer) + buf->len);
  // new_buf->data = (uint8_t *)(new_buf + 1);
  // memcpy(new_buf->data, buf->data, buf->len);

  {
    std::lock_guard<std::mutex> lock(_send_queue_mtx);
    _send_queue.push(new_buf);
  }

  // Signal the IO thread that there's data to send
  uint64_t val = 1;
  if (write(_signal_fd, &val, sizeof(val)) < 0) {
    log_error("RetransmissionClient: Failed to write to signal_fd: %m");
  }

  return 0;
}

void RetransmissionClient::_io_loop_thread() {
  log_info("RetransmissionClient: IO thread started.");

  const int max_events = 2;  // Socket fd and signal fd
  struct epoll_event events[max_events];

  while (_running.load(std::memory_order_acquire)) {
    int nfds =
        epoll_wait(_epoll_fd, events, max_events, -1);  // Wait indefinitely
    if (nfds < 0) {
      if (errno == EINTR) {
        continue;
      }
      log_error("RetransmissionClient: epoll_wait error: %m");
      break;
    }

    for (int i = 0; i < nfds; ++i) {
      if (events[i].data.fd == _signal_fd) {
        // Consume the signal
        uint64_t val;
        if (read(_signal_fd, &val, sizeof(val)) < 0 && errno != EAGAIN) {
          log_error("RetransmissionClient: Error reading from signal_fd: %m");
        }
        if (!_running.load(std::memory_order_acquire)) {
          break;  // Exit loop if stop() was called
        }
        // New messages to send or stop signal received
        _send_queued_messages();
      } else if (events[i].data.fd == _fd) {
        if (events[i].events & EPOLLIN) {
          handle_read();
        }
        if (events[i].events & EPOLLOUT) {
          _send_queued_messages();
        }
      }
    }

    // Reconnect if disconnected and still running
    if (_fd < 0 && _running.load(std::memory_order_acquire)) {
      log_info("RetransmissionClient: Attempting to reconnect...");
      _connect();
      // Short delay before next reconnection attempt to prevent busy-loop
      if (_fd < 0) {
        usleep(1000 * 1000);  // 1 second
      }
    }
  }

  log_info("RetransmissionClient: IO thread stopped.");
}

int RetransmissionClient::_connect() {
  _disconnect();  // Ensure previous connection is closed

  _fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (_fd < 0) {
    log_error("RetransmissionClient: Failed to create socket: %m");
    return -1;
  }

  if (::connect(_fd, (struct sockaddr *)&_addr, sizeof(_addr)) < 0) {
    if (errno != EINPROGRESS) {
      log_warning(
          "RetransmissionClient: Failed to connect to server on port %d: %m",
          RETRANSMISSION_CLIENT_PORT);
      close(_fd);
      _fd = -1;
      return -1;
    }
  }

  // Add socket to epoll instance for monitoring read and write events
  struct epoll_event epev = {};
  epev.events = EPOLLIN | EPOLLOUT | EPOLLET;  // Edge-triggered
  epev.data.fd = _fd;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _fd, &epev) < 0) {
    log_error("RetransmissionClient: Failed to add socket to epoll: %m");
    close(_fd);
    _fd = -1;
    return -1;
  }

  log_info("RetransmissionClient: Connected to server on port %d",
           RETRANSMISSION_CLIENT_PORT);
  return 0;
}

void RetransmissionClient::_disconnect() {
  if (_fd >= 0) {
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, _fd, nullptr);  // Remove from epoll
    close(_fd);
    _fd = -1;
    log_info("RetransmissionClient: Disconnected from server.");
  }
}

void RetransmissionClient::_send_queued_messages() {
  std::lock_guard<std::mutex> lock(_send_queue_mtx);

  if (_fd < 0) {
    // If not connected, cannot send. Clear queue for now or re-queue later.
    while (!_send_queue.empty()) {
      // delete _send_queue.front();
      _send_queue.pop();
    }
    return;
  }

  while (!_send_queue.empty()) {
    std::vector<uint8_t> buf = _send_queue.front();

    uint32_t network_sender_id = htonl(_sender_id);
    // uint8_t msg_type = buf->data[1];

    std::vector<iovec> iov(1);

    iov[0].iov_base = buf.data();
    iov[0].iov_len = buf.size();

    ssize_t n = writev(_fd, iov.data(), iov.size());
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Not all data could be sent, wait for EPOLLOUT
        log_debug("RetransmissionClient: writev would block.");
        struct epoll_event epev = {};
        epev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        epev.data.fd = _fd;
        epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _fd, &epev);
        return;  // Try again when EPOLLOUT is triggered
      } else {
        log_error("RetransmissionClient: Error writing to socket: %m");
        _disconnect();
        // Discard message and others in queue until reconnected
        while (!_send_queue.empty()) {
          // delete _send_queue.front();
          _send_queue.pop();
        }
        return;
      }
    } else if ((size_t)n < buf.size()) {
      // Partial write, for simplicity, we'll discard and log. A more robust
      // solution would handle partial writes.
      log_warning("RetransmissionClient: Partial write, message discarded.");
      // delete buf;
      _send_queue.pop();
      // Also, update epoll to wait for EPOLLOUT if needed.
      struct epoll_event epev = {};
      epev.events = EPOLLIN | EPOLLOUT | EPOLLET;
      epev.data.fd = _fd;
      epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _fd, &epev);
      return;
    }

    // delete buf; // Message sent successfully
    _send_queue.pop();
  }

  // If queue is empty, remove EPOLLOUT to avoid unnecessary wakeups
  struct epoll_event epev = {};
  epev.events = EPOLLIN | EPOLLET;
  epev.data.fd = _fd;
  epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _fd, &epev);
}
