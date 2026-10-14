#include "cansat_comms/xbee_frame.hpp"

namespace cansat::xbee {
namespace {

void write_be(uint8_t* dst, uint64_t value, size_t num_bytes) {
    for (size_t i = 0; i < num_bytes; ++i) {
        dst[i] = static_cast<uint8_t>(value >> (8 * (num_bytes - 1 - i)));
    }
}

uint64_t read_be(const uint8_t* src, size_t num_bytes) {
    uint64_t value = 0;
    for (size_t i = 0; i < num_bytes; ++i) {
        value = (value << 8) | src[i];
    }
    return value;
}

}  // namespace

uint8_t compute_checksum(ByteView frame_data) {
    uint8_t sum = 0;  // uint8_t arithmetic wraps, i.e. keeps the low 8 bits
    for (size_t i = 0; i < frame_data.size; ++i) {
        sum = static_cast<uint8_t>(sum + frame_data.data[i]);
    }
    return static_cast<uint8_t>(0xFF - sum);
}

Status build_transmit_request(const TransmitOptions& options, ByteView payload,
                              uint8_t* out, size_t out_capacity, size_t& out_size) {
    out_size = 0;
    if (payload.size > 0 && payload.data == nullptr) return Status::InvalidArgument;
    if (payload.size > kMaxRfPayload) return Status::PayloadTooLarge;

    const size_t frame_data_len = kTransmitHeaderSize + payload.size;
    const size_t total = kFrameOverhead + frame_data_len;
    if (out == nullptr || out_capacity < total) return Status::BufferTooSmall;

    size_t i = 0;
    out[i++] = kStartDelimiter;
    out[i++] = static_cast<uint8_t>(frame_data_len >> 8);
    out[i++] = static_cast<uint8_t>(frame_data_len & 0xFF);

    const size_t frame_data_start = i;
    out[i++] = kFrameTypeTransmitRequest;
    out[i++] = options.frame_id;
    write_be(&out[i], options.dest_address64, 8);
    i += 8;
    write_be(&out[i], options.dest_address16, 2);
    i += 2;
    out[i++] = options.broadcast_radius;
    out[i++] = options.options;
    for (size_t p = 0; p < payload.size; ++p) {
        out[i++] = payload.data[p];
    }

    out[i] = compute_checksum(ByteView(&out[frame_data_start], frame_data_len));
    out_size = total;
    return Status::Ok;
}

Status validate_frame(ByteView frame, ByteView& frame_data) {
    frame_data = ByteView();
    if (frame.size > 0 && frame.data == nullptr) return Status::InvalidArgument;
    if (frame.size == 0) return Status::Incomplete;
    if (frame[0] != kStartDelimiter) return Status::BadStartDelimiter;
    if (frame.size < 3) return Status::Incomplete;  // need the length field

    const size_t length = (static_cast<size_t>(frame[1]) << 8) | frame[2];
    if (length == 0) return Status::LengthMismatch;  // no frame type byte

    const size_t total = kFrameOverhead + length;
    if (frame.size < total) return Status::Incomplete;
    if (frame.size > total) return Status::LengthMismatch;

    // Valid iff sum(frame data) + checksum == 0xFF (mod 256).
    uint8_t sum = 0;
    for (size_t i = 3; i < total; ++i) {
        sum = static_cast<uint8_t>(sum + frame[i]);
    }
    if (sum != 0xFF) return Status::BadChecksum;

    frame_data = ByteView(frame.data + 3, length);
    return Status::Ok;
}

Status parse_receive_packet(ByteView frame, ReceivePacket& out) {
    ByteView data;
    const Status status = validate_frame(frame, data);
    if (status != Status::Ok) return status;

    if (data[0] != kFrameTypeReceivePacket) return Status::UnexpectedFrameType;
    if (data.size < kReceiveHeaderSize) return Status::MalformedFrame;
    if (data.size - kReceiveHeaderSize > kMaxRfPayload) return Status::PayloadTooLarge;

    out.source_address64 = read_be(&data.data[1], 8);
    out.source_address16 = static_cast<uint16_t>(read_be(&data.data[9], 2));
    out.receive_options = data[11];
    out.payload = ByteView(data.data + kReceiveHeaderSize, data.size - kReceiveHeaderSize);
    return Status::Ok;
}

}  // namespace cansat::xbee
