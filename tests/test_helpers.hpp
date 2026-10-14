#pragma once

// Test-only helpers. These run on the development PC, so std::vector is fine
// here; the library itself never allocates.

#include <cstdint>
#include <vector>

#include "cansat_comms/common.hpp"
#include "cansat_comms/xbee_frame.hpp"

namespace test_helpers {

inline cansat::ByteView view(const std::vector<uint8_t>& v) {
    return cansat::ByteView(v.data(), v.size());
}

/// Independently builds a 0x90 Receive Packet frame (as the XBee module would
/// emit it), computing length and checksum by hand rather than via the library.
inline std::vector<uint8_t> make_receive_frame(const std::vector<uint8_t>& rf_data,
                                               uint64_t src64 = 0x0013A20040522BAAULL,
                                               uint16_t src16 = 0x7D84,
                                               uint8_t options = 0x01) {
    std::vector<uint8_t> data;
    data.push_back(0x90);
    for (int i = 7; i >= 0; --i) data.push_back(static_cast<uint8_t>(src64 >> (8 * i)));
    data.push_back(static_cast<uint8_t>(src16 >> 8));
    data.push_back(static_cast<uint8_t>(src16));
    data.push_back(options);
    data.insert(data.end(), rf_data.begin(), rf_data.end());

    unsigned sum = 0;
    for (uint8_t b : data) sum += b;

    std::vector<uint8_t> frame;
    frame.push_back(0x7E);
    frame.push_back(static_cast<uint8_t>(data.size() >> 8));
    frame.push_back(static_cast<uint8_t>(data.size()));
    frame.insert(frame.end(), data.begin(), data.end());
    frame.push_back(static_cast<uint8_t>(0xFF - (sum & 0xFF)));
    return frame;
}

/// Recomputes the checksum byte of a frame after a test has edited its contents.
inline void fix_checksum(std::vector<uint8_t>& frame) {
    unsigned sum = 0;
    for (size_t i = 3; i + 1 < frame.size(); ++i) sum += frame[i];
    frame.back() = static_cast<uint8_t>(0xFF - (sum & 0xFF));
}

}  // namespace test_helpers
