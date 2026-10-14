#pragma once

// Application payload format (carried in the XBee RF data field).
//
// Every payload = 1-byte message type + fixed-size body.
// Multi-byte integers are big-endian (network byte order).
//
//   OutTelemetry      (7 bytes): [0x01][seconds_since_epoch u32 BE][mechanisms_deployed_flags u16 BE]
//   CommandMechanism  (3 bytes): [0x02][mechanism_id u8][value u8]
//   CommandTelemetry  (2 bytes): [0x03][is_on u8: 0x00 = false, 0x01 = true]
//
// Payloads must be exactly the documented length; anything shorter is
// Incomplete, anything longer is BadPayloadLength. This layer knows nothing
// about XBee frames, addresses or checksums.

#include "cansat_comms/common.hpp"
#include "cansat_comms/messages.hpp"

namespace cansat::payload {

enum class MessageType : uint8_t {
    OutTelemetry = 0x01,
    CommandMechanism = 0x02,
    CommandTelemetry = 0x03,
};

inline constexpr size_t kOutTelemetrySize = 7;
inline constexpr size_t kCommandMechanismSize = 3;
inline constexpr size_t kCommandTelemetrySize = 2;
inline constexpr size_t kMaxPayloadSize = kOutTelemetrySize;

/// Reads and validates the message-type byte.
Status peek_message_type(ByteView payload, MessageType& type);

/// Serializes telemetry into `out`. On success `written` == kOutTelemetrySize.
Status serialize(const OutTelemetry& msg, uint8_t* out, size_t out_capacity, size_t& written);

/// Deserializers. `out` is only modified on success.
Status deserialize(ByteView payload, CommandMechanism& out);
Status deserialize(ByteView payload, CommandTelemetry& out);
Status deserialize(ByteView payload, OutTelemetry& out);  // ground-station side / tests

/// A command received from the Ground Station: whichever of the two
/// command types the payload contained (selected by `type`).
struct GroundCommand {
    MessageType type = MessageType::CommandMechanism;
    CommandMechanism mechanism = {};
    CommandTelemetry telemetry = {};
};

/// Identifies and decodes an uplinked command payload in one step.
/// Rejects OutTelemetry (downlink-only) with UnexpectedMessageType.
Status decode_ground_command(ByteView payload, GroundCommand& out);

}  // namespace cansat::payload
