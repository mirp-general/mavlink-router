#ifndef CHANNEL_STATS_PACKET_H_
#define CHANNEL_STATS_PACKET_H_

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

namespace {

// Helper function to serialize a uint32_t in little-endian order.
void SerializeUint32(uint32_t value, std::vector<uint8_t>* buffer) {
  buffer->push_back(static_cast<uint8_t>(value & 0xFF));
  buffer->push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  buffer->push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  buffer->push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

// Helper function to deserialize a uint32_t in little-endian order.
uint32_t DeserializeUint32(const uint8_t* buffer, size_t* pos) {
  uint32_t value = static_cast<uint32_t>(buffer[*pos]) |
                   (static_cast<uint32_t>(buffer[*pos + 1]) << 8) |
                   (static_cast<uint32_t>(buffer[*pos + 2]) << 16) |
                   (static_cast<uint32_t>(buffer[*pos + 3]) << 24);
  *pos += 4;
  return value;
}

// Helper function to serialize a float.
void SerializeFloat(float value, std::vector<uint8_t>* buffer) {
    uint8_t bytes[sizeof(float)];
    memcpy(bytes, &value, sizeof(float));
    for (size_t i = 0; i < sizeof(float); ++i) {
        buffer->push_back(bytes[i]);
    }
}

// Helper function to deserialize a float.
float DeserializeFloat(const uint8_t* buffer, size_t* pos) {
    float value;
    memcpy(&value, buffer + *pos, sizeof(float));
    *pos += sizeof(float);
    return value;
}

// Helper function to serialize a string as uint8_t length followed by content.
void SerializeString(const std::string& str, std::vector<uint8_t>* buffer) {
  uint8_t len = static_cast<uint8_t>(str.size());
  buffer->push_back(len);
  buffer->insert(buffer->end(), str.begin(), str.end());
}

// Helper function to deserialize a string: uint8_t length followed by content.
std::string DeserializeString(const uint8_t* buffer, size_t* pos) {
  uint8_t len = buffer[*pos];
  ++(*pos);
  std::string str(reinterpret_cast<const char*>(buffer + *pos), len);
  *pos += len;
  return str;
}

}  // namespace

struct ChannelStatsPacket {
  uint32_t sender_id_;
  uint8_t type_id_;
  std::string e_type_;
  std::string e_name_;
  std::string e_group_name_;
  bool is_active_;
  float incoming_bitrate_kbps_;
  float outgoing_bitrate_kbps_;
  float incoming_jitter_ms_;
  float signal_level_;
  float noise_level_;

  // Serializes the struct into a vector<uint8_t> in the order of fields.
  // Assumes all lengths fit in uint8_t (i.e., < 256).
  // Uses little-endian for uint32_t.
  std::vector<uint8_t> Serialize() const {
    std::vector<uint8_t> buffer;
    SerializeUint32(sender_id_, &buffer);
    buffer.push_back(type_id_);
    SerializeString(e_type_, &buffer);
    SerializeString(e_name_, &buffer);
    SerializeString(e_group_name_, &buffer);
    buffer.push_back(static_cast<uint8_t>(is_active_));
    SerializeFloat(incoming_bitrate_kbps_, &buffer);
    SerializeFloat(outgoing_bitrate_kbps_, &buffer);
    SerializeFloat(incoming_jitter_ms_, &buffer);
    SerializeFloat(signal_level_, &buffer);
    SerializeFloat(noise_level_, &buffer);
    return buffer;
  }
};

#endif  // CHANNEL_STATS_PACKET_H_
