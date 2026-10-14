#pragma once

// Application-level message structures (as given in the assignment).
// These are plain in-memory types; their wire format is defined in payload.hpp,
// NOT by their memory layout.

#include <cstdint>

namespace cansat {

/// Ground Station -> CanSat: actuate a mechanism.
struct CommandMechanism {
    uint8_t mechanism_id;
    uint8_t value;
};

/// Ground Station -> CanSat: enable / disable telemetry.
struct CommandTelemetry {
    bool is_on;
};

/// CanSat -> Ground Station: periodic telemetry.
struct OutTelemetry {
    uint32_t seconds_since_epoch;
    uint16_t mechanisms_deployed_flags;
};

}  // namespace cansat
