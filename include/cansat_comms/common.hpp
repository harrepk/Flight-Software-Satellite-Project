#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cansat {

/// Result of every fallible operation in the library.
/// No exceptions are used, so callers on a microcontroller can always
/// check (and log/count) exactly why something was rejected.
enum class Status : uint8_t {
    Ok = 0,
    InvalidArgument,       // null pointer with non-zero size, etc.
    BufferTooSmall,        // caller-provided output buffer cannot hold the result
    PayloadTooLarge,       // payload exceeds the XBee RF payload limit
    Incomplete,            // fewer bytes than the frame / payload requires
    BadStartDelimiter,     // first byte of a frame is not 0x7E
    LengthMismatch,        // length field disagrees with the bytes provided
    BadChecksum,           // XBee checksum does not validate
    UnexpectedFrameType,   // valid frame, but not the API frame type requested
    MalformedFrame,        // valid checksum, but frame too short for its type
    UnknownMessageType,    // payload message ID not defined in the spec
    UnexpectedMessageType, // valid message ID, but not the one requested
    BadPayloadLength,      // payload longer than the message it claims to be
    InvalidFieldValue,     // field outside its allowed range (e.g. bool not 0/1)
};

/// Human-readable name for a Status (for logs / test output).
const char* to_string(Status status);

/// Non-owning, read-only view over a contiguous byte range.
/// (A minimal stand-in for std::span<const uint8_t>, which needs C++20.)
struct ByteView {
    const uint8_t* data = nullptr;
    size_t size = 0;

    constexpr ByteView() = default;
    constexpr ByteView(const uint8_t* d, size_t n) : data(d), size(n) {}

    template <size_t N>
    constexpr ByteView(const uint8_t (&arr)[N]) : data(arr), size(N) {}

    template <size_t N>
    constexpr ByteView(const std::array<uint8_t, N>& arr) : data(arr.data()), size(N) {}

    constexpr bool empty() const { return size == 0; }
    constexpr uint8_t operator[](size_t i) const { return data[i]; }
};

}  // namespace cansat
