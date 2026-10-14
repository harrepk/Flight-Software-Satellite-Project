#include "cansat_comms/payload.hpp"

namespace cansat::payload {
namespace {

bool is_known_type(uint8_t raw) {
    return raw == static_cast<uint8_t>(MessageType::OutTelemetry) ||
           raw == static_cast<uint8_t>(MessageType::CommandMechanism) ||
           raw == static_cast<uint8_t>(MessageType::CommandTelemetry);
}

// Common checks for a fixed-size message: valid view, correct type, exact length.
Status check_header(ByteView payload, MessageType expected, size_t expected_size) {
    MessageType type;
    const Status status = peek_message_type(payload, type);
    if (status != Status::Ok) return status;
    if (type != expected) return Status::UnexpectedMessageType;
    if (payload.size < expected_size) return Status::Incomplete;
    if (payload.size > expected_size) return Status::BadPayloadLength;
    return Status::Ok;
}

}  // namespace

Status peek_message_type(ByteView payload, MessageType& type) {
    if (payload.size > 0 && payload.data == nullptr) return Status::InvalidArgument;
    if (payload.size == 0) return Status::Incomplete;
    if (!is_known_type(payload[0])) return Status::UnknownMessageType;
    type = static_cast<MessageType>(payload[0]);
    return Status::Ok;
}

Status serialize(const OutTelemetry& msg, uint8_t* out, size_t out_capacity, size_t& written) {
    written = 0;
    if (out == nullptr || out_capacity < kOutTelemetrySize) return Status::BufferTooSmall;

    const uint32_t t = msg.seconds_since_epoch;
    const uint16_t f = msg.mechanisms_deployed_flags;
    out[0] = static_cast<uint8_t>(MessageType::OutTelemetry);
    out[1] = static_cast<uint8_t>(t >> 24);
    out[2] = static_cast<uint8_t>(t >> 16);
    out[3] = static_cast<uint8_t>(t >> 8);
    out[4] = static_cast<uint8_t>(t);
    out[5] = static_cast<uint8_t>(f >> 8);
    out[6] = static_cast<uint8_t>(f);
    written = kOutTelemetrySize;
    return Status::Ok;
}

Status deserialize(ByteView payload, CommandMechanism& out) {
    const Status status = check_header(payload, MessageType::CommandMechanism, kCommandMechanismSize);
    if (status != Status::Ok) return status;
    out.mechanism_id = payload[1];
    out.value = payload[2];
    return Status::Ok;
}

Status deserialize(ByteView payload, CommandTelemetry& out) {
    const Status status = check_header(payload, MessageType::CommandTelemetry, kCommandTelemetrySize);
    if (status != Status::Ok) return status;
    // Strict boolean: anything other than 0/1 suggests corruption or a
    // mismatched protocol version, so reject rather than guess.
    if (payload[1] > 0x01) return Status::InvalidFieldValue;
    out.is_on = (payload[1] == 0x01);
    return Status::Ok;
}

Status deserialize(ByteView payload, OutTelemetry& out) {
    const Status status = check_header(payload, MessageType::OutTelemetry, kOutTelemetrySize);
    if (status != Status::Ok) return status;
    out.seconds_since_epoch = (static_cast<uint32_t>(payload[1]) << 24) |
                              (static_cast<uint32_t>(payload[2]) << 16) |
                              (static_cast<uint32_t>(payload[3]) << 8) |
                              static_cast<uint32_t>(payload[4]);
    out.mechanisms_deployed_flags =
        static_cast<uint16_t>((static_cast<uint16_t>(payload[5]) << 8) | payload[6]);
    return Status::Ok;
}

Status decode_ground_command(ByteView payload, GroundCommand& out) {
    MessageType type;
    Status status = peek_message_type(payload, type);
    if (status != Status::Ok) return status;

    GroundCommand result;
    result.type = type;
    switch (type) {
        case MessageType::CommandMechanism:
            status = deserialize(payload, result.mechanism);
            break;
        case MessageType::CommandTelemetry:
            status = deserialize(payload, result.telemetry);
            break;
        case MessageType::OutTelemetry:
            return Status::UnexpectedMessageType;  // downlink-only message
    }
    if (status == Status::Ok) out = result;
    return status;
}

}  // namespace cansat::payload
