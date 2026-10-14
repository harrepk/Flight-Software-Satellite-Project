#pragma once

// Incremental frame reader for a UART byte stream.
//
// On the flight computer bytes arrive one at a time (typically from a UART
// RX interrupt or DMA ring buffer). FrameReader assembles them into complete
// frames in a fixed internal buffer and re-synchronises on the next 0x7E
// after any error, so line noise or a dropped byte only costs one frame.

#include "cansat_comms/common.hpp"
#include "cansat_comms/xbee_frame.hpp"

namespace cansat::xbee {

class FrameReader {
public:
    enum class Result : uint8_t {
        NeedMore,    // byte consumed, frame not finished yet
        FrameReady,  // a complete, checksum-valid frame is available via frame()
        Error,       // frame discarded; see last_error(). Reader is ready for the next 0x7E
    };

    /// Largest frame accepted. Anything bigger is discarded as an error.
    static constexpr size_t kCapacity = kMaxReceiveFrameSize;

    /// Feed one received byte.
    Result push(uint8_t byte);

    /// The most recent complete frame (delimiter through checksum).
    /// Valid after push() returned FrameReady, until the next push().
    ByteView frame() const { return ByteView(buffer_, frame_ready_ ? count_ : 0); }

    Status last_error() const { return last_error_; }

    /// Number of frames discarded since construction / reset().
    uint32_t error_count() const { return error_count_; }

    void reset();

private:
    enum class State : uint8_t { WaitStart, LengthHigh, LengthLow, Body };

    Result fail(Status reason);

    uint8_t buffer_[kCapacity] = {};
    size_t count_ = 0;
    size_t expected_total_ = 0;
    State state_ = State::WaitStart;
    bool frame_ready_ = false;
    Status last_error_ = Status::Ok;
    uint32_t error_count_ = 0;
};

}  // namespace cansat::xbee
