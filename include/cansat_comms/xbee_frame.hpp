#pragma once

// XBee API framing (API mode 1, AP=1: no byte escaping).
//
//   +------+--------+--------+-------------------------+----------+
//   | 0x7E | LEN_HI | LEN_LO |  frame data (LEN bytes) | checksum |
//   +------+--------+--------+-------------------------+----------+
//
//   LEN      = number of frame-data bytes (frame type .. last RF byte), big-endian
//   checksum = 0xFF - (sum of frame-data bytes & 0xFF)
//
// This layer knows nothing about what the RF payload contains.

#include "cansat_comms/common.hpp"

namespace cansat::xbee {

inline constexpr uint8_t kStartDelimiter = 0x7E;
inline constexpr uint8_t kFrameTypeTransmitRequest = 0x10;
inline constexpr uint8_t kFrameTypeReceivePacket = 0x90;

/// Maximum RF payload accepted by this library. 256 bytes is the NP limit
/// on XBee-PRO 900HP / 9XR-class modules; our largest message is 7 bytes,
/// so this is a generous upper bound that keeps buffers statically sized.
inline constexpr size_t kMaxRfPayload = 256;

/// Start delimiter + 2 length bytes + checksum.
inline constexpr size_t kFrameOverhead = 4;
/// Frame type, frame ID, 64-bit dest, 16-bit dest, broadcast radius, options.
inline constexpr size_t kTransmitHeaderSize = 14;
/// Frame type, 64-bit source, 16-bit source, receive options.
inline constexpr size_t kReceiveHeaderSize = 12;

inline constexpr size_t kMaxTransmitFrameSize =
    kFrameOverhead + kTransmitHeaderSize + kMaxRfPayload;
inline constexpr size_t kMaxReceiveFrameSize =
    kFrameOverhead + kReceiveHeaderSize + kMaxRfPayload;

inline constexpr uint64_t kCoordinatorAddress64 = 0x0000000000000000ULL;
inline constexpr uint64_t kBroadcastAddress64 = 0x000000000000FFFFULL;
inline constexpr uint16_t kUnknownAddress16 = 0xFFFE;

/// Addressing / option fields of a 0x10 Transmit Request.
struct TransmitOptions {
    uint8_t frame_id = 0x01;  // 0 disables the 0x8B Transmit Status response
    uint64_t dest_address64 = kCoordinatorAddress64;
    uint16_t dest_address16 = kUnknownAddress16;
    uint8_t broadcast_radius = 0x00;  // 0 = maximum hops
    uint8_t options = 0x00;
};

/// Result of parsing a 0x90 Receive Packet.
/// `payload` points INTO the caller's frame buffer (zero-copy), so it is
/// only valid while that buffer is unchanged.
struct ReceivePacket {
    uint64_t source_address64 = 0;
    uint16_t source_address16 = 0;
    uint8_t receive_options = 0;
    ByteView payload;
};

/// XBee checksum of the frame-data bytes: 0xFF - (sum & 0xFF).
uint8_t compute_checksum(ByteView frame_data);

/// Builds a complete 0x10 Transmit Request frame (delimiter through checksum)
/// around `payload` into `out`. On success `out_size` is the frame length.
/// Never writes past `out_capacity`; on failure `out_size` is 0.
Status build_transmit_request(const TransmitOptions& options, ByteView payload,
                              uint8_t* out, size_t out_capacity, size_t& out_size);

/// Checks delimiter, length field and checksum of one complete frame and
/// returns a view of its frame data (frame type .. last data byte).
/// `frame` must contain exactly one frame: trailing bytes are rejected.
Status validate_frame(ByteView frame, ByteView& frame_data);

/// Parses one complete 0x90 Receive Packet frame. On success `out.payload`
/// views the RF data inside `frame`.
Status parse_receive_packet(ByteView frame, ReceivePacket& out);

}  // namespace cansat::xbee
