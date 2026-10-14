#include "cansat_comms/xbee_frame_reader.hpp"

namespace cansat::xbee {

void FrameReader::reset() {
    count_ = 0;
    expected_total_ = 0;
    state_ = State::WaitStart;
    frame_ready_ = false;
    last_error_ = Status::Ok;
    error_count_ = 0;
}

FrameReader::Result FrameReader::fail(Status reason) {
    last_error_ = reason;
    ++error_count_;
    count_ = 0;
    state_ = State::WaitStart;
    return Result::Error;
}

FrameReader::Result FrameReader::push(uint8_t byte) {
    frame_ready_ = false;

    switch (state_) {
        case State::WaitStart:
            // Bytes before a start delimiter are line noise: skip silently.
            if (byte != kStartDelimiter) return Result::NeedMore;
            count_ = 0;
            buffer_[count_++] = byte;
            state_ = State::LengthHigh;
            return Result::NeedMore;

        case State::LengthHigh:
            buffer_[count_++] = byte;
            state_ = State::LengthLow;
            return Result::NeedMore;

        case State::LengthLow: {
            buffer_[count_++] = byte;
            const size_t length = (static_cast<size_t>(buffer_[1]) << 8) | buffer_[2];
            if (length == 0) return fail(Status::LengthMismatch);
            expected_total_ = kFrameOverhead + length;
            if (expected_total_ > kCapacity) return fail(Status::PayloadTooLarge);
            state_ = State::Body;
            return Result::NeedMore;
        }

        case State::Body: {
            buffer_[count_++] = byte;
            if (count_ < expected_total_) return Result::NeedMore;

            ByteView unused;
            const Status status = validate_frame(ByteView(buffer_, count_), unused);
            state_ = State::WaitStart;
            if (status != Status::Ok) return fail(status);
            frame_ready_ = true;
            return Result::FrameReady;
        }
    }
    return fail(Status::InvalidArgument);  // unreachable
}

}  // namespace cansat::xbee
