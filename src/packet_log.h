#ifndef PACKET_LOG_H_
#define PACKET_LOG_H_

#include <cstdint>
#include <string>
#include <vector>

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

// Helper function to serialize payload as uint8_t length followed by content.
void SerializePayload(const std::vector<uint8_t>& payload,
                      std::vector<uint8_t>* buffer) {
  uint8_t len = static_cast<uint8_t>(payload.size());
  buffer->push_back(len);
  buffer->insert(buffer->end(), payload.begin(), payload.end());
}

// Helper function to deserialize payload: uint8_t length followed by content.
std::vector<uint8_t> DeserializePayload(const uint8_t* buffer, size_t* pos) {
  uint8_t len = buffer[*pos];
  ++(*pos);
  std::vector<uint8_t> payload(buffer + *pos, buffer + *pos + len);
  *pos += len;
  return payload;
}

}  // namespace

struct PacketLog {
  uint32_t sender_id_;
  uint8_t type_id_;
  uint8_t msg_id_;
  uint8_t target_sysid_;
  uint8_t target_compid_;
  uint8_t src_sysid_;
  uint8_t src_compid_;
  std::string e_type_;
  std::string e_name_;
  std::string e_group_name_;
  std::vector<uint8_t> payload_;  // payload_len_ is derived from payload_.size()

  // Serializes the struct into a vector<uint8_t> in the order of fields.
  // Assumes all lengths fit in uint8_t (i.e., < 256).
  // Uses little-endian for uint32_t.
  std::vector<uint8_t> Serialize() const {
    std::vector<uint8_t> buffer;
    SerializeUint32(sender_id_, &buffer);
    buffer.push_back(type_id_);
    buffer.push_back(msg_id_);
    buffer.push_back(target_sysid_);
    buffer.push_back(target_compid_);
    buffer.push_back(src_sysid_);
    buffer.push_back(src_compid_);
    SerializeString(e_type_, &buffer);
    SerializeString(e_name_, &buffer);
    SerializeString(e_group_name_, &buffer);
    SerializePayload(payload_, &buffer);
    return buffer;
  }

  // Deserializes from a buffer starting at pos, advancing pos.
  // Assumes the buffer has sufficient data; no error checking.
  // Uses little-endian for uint32_t.
  static PacketLog Deserialize(const uint8_t* buffer, size_t* pos) {
    PacketLog packet;
    packet.sender_id_ = DeserializeUint32(buffer, pos);
    packet.type_id_ = buffer[*pos];
    ++(*pos);
    packet.msg_id_ = buffer[*pos];
    ++(*pos);
    packet.target_sysid_ = buffer[*pos];
    ++(*pos);
    packet.target_compid_ = buffer[*pos];
    ++(*pos);
    packet.src_sysid_ = buffer[*pos];
    ++(*pos);
    packet.src_compid_ = buffer[*pos];
    ++(*pos);
    packet.e_type_ = DeserializeString(buffer, pos);
    packet.e_name_ = DeserializeString(buffer, pos);
    packet.e_group_name_ = DeserializeString(buffer, pos);
    packet.payload_ = DeserializePayload(buffer, pos);
    return packet;
  }
};

#endif  // PACKET_LOG_H_