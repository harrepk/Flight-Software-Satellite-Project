#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "cansat_comms/xbee_frame.hpp"
#include "test_helpers.hpp"

using namespace cansat;
using namespace cansat::xbee;
using test_helpers::make_receive_frame;
using test_helpers::view;

namespace {

// Example frames from the Digi XBee API frame documentation.
// 0x10: send "TxData0A" to 0013A200400A0127, 16-bit address unknown (FFFE).
const std::vector<uint8_t> kDigiTxExample = {
    0x7E, 0x00, 0x16, 0x10, 0x01, 0x00, 0x13, 0xA2, 0x00, 0x40, 0x0A, 0x01, 0x27,
    0xFF, 0xFE, 0x00, 0x00, 0x54, 0x78, 0x44, 0x61, 0x74, 0x61, 0x30, 0x41, 0x13};

// 0x90: "RxData" received from 0013A20040522BAA / 7D84, options 0x01 (ACKed).
const std::vector<uint8_t> kDigiRxExample = {
    0x7E, 0x00, 0x12, 0x90, 0x00, 0x13, 0xA2, 0x00, 0x40, 0x52, 0x2B, 0xAA,
    0x7D, 0x84, 0x01, 0x52, 0x78, 0x44, 0x61, 0x74, 0x61, 0x0D};

TransmitOptions digi_tx_options() {
    TransmitOptions opt;
    opt.frame_id = 0x01;
    opt.dest_address64 = 0x0013A200400A0127ULL;
    opt.dest_address16 = 0xFFFE;
    opt.broadcast_radius = 0x00;
    opt.options = 0x00;
    return opt;
}

const std::vector<uint8_t> kTxData0A = {'T', 'x', 'D', 'a', 't', 'a', '0', 'A'};

}  // namespace

// ----------------------------------------------------------------- checksum

TEST(Checksum, MatchesDigiExamples) {
    EXPECT_EQ(compute_checksum(ByteView(&kDigiTxExample[3], 0x16)), 0x13);
    EXPECT_EQ(compute_checksum(ByteView(&kDigiRxExample[3], 0x12)), 0x0D);
}

TEST(Checksum, EmptyDataIsFF) { EXPECT_EQ(compute_checksum(ByteView()), 0xFF); }

TEST(Checksum, WrapsModulo256) {
    const uint8_t data[] = {0xFF, 0xFF, 0x03};  // sum = 0x201 -> low byte 0x01
    EXPECT_EQ(compute_checksum(data), 0xFE);
}

// ------------------------------------------------------ 0x10 construction

TEST(TransmitRequest, MatchesDigiExampleByteForByte) {
    std::array<uint8_t, kMaxTransmitFrameSize> out{};
    size_t size = 0;
    ASSERT_EQ(build_transmit_request(digi_tx_options(), view(kTxData0A), out.data(), out.size(), size),
              Status::Ok);
    ASSERT_EQ(size, kDigiTxExample.size());
    EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.begin() + static_cast<long>(size)), kDigiTxExample);
}

TEST(TransmitRequest, DefaultOptionsTargetCoordinator) {
    const uint8_t payload[] = {0xAB};
    std::array<uint8_t, kMaxTransmitFrameSize> out{};
    size_t size = 0;
    ASSERT_EQ(build_transmit_request(TransmitOptions{}, payload, out.data(), out.size(), size), Status::Ok);
    const std::vector<uint8_t> expected = {0x7E, 0x00, 0x0F, 0x10, 0x01, 0, 0, 0, 0, 0, 0, 0, 0,
                                           0xFF, 0xFE, 0x00, 0x00, 0xAB, 0x00};
    std::vector<uint8_t> got(out.begin(), out.begin() + static_cast<long>(size));
    // checksum: 0xFF - ((0x10+0x01+0xFF+0xFE+0xAB) & 0xFF) = 0xFF - 0xB9 = 0x46
    std::vector<uint8_t> exp = expected;
    exp.back() = 0x46;
    EXPECT_EQ(got, exp);
}

TEST(TransmitRequest, LengthFieldCountsFrameDataOnly) {
    const std::vector<uint8_t> payload(5, 0x00);
    std::array<uint8_t, kMaxTransmitFrameSize> out{};
    size_t size = 0;
    ASSERT_EQ(build_transmit_request(TransmitOptions{}, view(payload), out.data(), out.size(), size), Status::Ok);
    EXPECT_EQ(out[1], 0x00);
    EXPECT_EQ(out[2], kTransmitHeaderSize + 5);
    EXPECT_EQ(size, kFrameOverhead + kTransmitHeaderSize + 5);
}

TEST(TransmitRequest, LargePayloadUsesHighLengthByte) {
    const std::vector<uint8_t> payload(kMaxRfPayload, 0x55);
    std::array<uint8_t, kMaxTransmitFrameSize> out{};
    size_t size = 0;
    ASSERT_EQ(build_transmit_request(TransmitOptions{}, view(payload), out.data(), out.size(), size), Status::Ok);
    const size_t len = kTransmitHeaderSize + kMaxRfPayload;  // 270 = 0x010E
    EXPECT_EQ(out[1], static_cast<uint8_t>(len >> 8));
    EXPECT_EQ(out[2], static_cast<uint8_t>(len & 0xFF));
    EXPECT_EQ(size, kMaxTransmitFrameSize);
    ByteView data;
    EXPECT_EQ(validate_frame(ByteView(out.data(), size), data), Status::Ok);
}

TEST(TransmitRequest, EmptyPayloadIsAllowed) {
    std::array<uint8_t, kMaxTransmitFrameSize> out{};
    size_t size = 0;
    ASSERT_EQ(build_transmit_request(TransmitOptions{}, ByteView(), out.data(), out.size(), size), Status::Ok);
    EXPECT_EQ(size, kFrameOverhead + kTransmitHeaderSize);
}

TEST(TransmitRequest, RejectsOversizedPayload) {
    const std::vector<uint8_t> payload(kMaxRfPayload + 1, 0x00);
    std::array<uint8_t, kMaxTransmitFrameSize + 1> out{};
    size_t size = 99;
    EXPECT_EQ(build_transmit_request(TransmitOptions{}, view(payload), out.data(), out.size(), size),
              Status::PayloadTooLarge);
    EXPECT_EQ(size, 0u);
}

TEST(TransmitRequest, OutputBufferBoundaryIsExact) {
    const size_t needed = kDigiTxExample.size();
    std::vector<uint8_t> out(needed + 4, 0xCC);  // 4 sentinel bytes past the end
    size_t size = 0;

    EXPECT_EQ(build_transmit_request(digi_tx_options(), view(kTxData0A), out.data(), needed - 1, size),
              Status::BufferTooSmall);
    EXPECT_EQ(size, 0u);
    for (uint8_t b : out) EXPECT_EQ(b, 0xCC) << "nothing may be written on failure";

    EXPECT_EQ(build_transmit_request(digi_tx_options(), view(kTxData0A), out.data(), needed, size), Status::Ok);
    for (size_t i = needed; i < out.size(); ++i) EXPECT_EQ(out[i], 0xCC) << "wrote past capacity at " << i;
}

TEST(TransmitRequest, RejectsNullPointers) {
    std::array<uint8_t, kMaxTransmitFrameSize> out{};
    size_t size = 0;
    EXPECT_EQ(build_transmit_request(TransmitOptions{}, ByteView(nullptr, 3), out.data(), out.size(), size),
              Status::InvalidArgument);
    EXPECT_EQ(build_transmit_request(TransmitOptions{}, view(kTxData0A), nullptr, 100, size),
              Status::BufferTooSmall);
}

// ---------------------------------------------------------- 0x90 parsing

TEST(ReceivePacket, ParsesDigiExample) {
    ReceivePacket rx;
    ASSERT_EQ(parse_receive_packet(view(kDigiRxExample), rx), Status::Ok);
    EXPECT_EQ(rx.source_address64, 0x0013A20040522BAAULL);
    EXPECT_EQ(rx.source_address16, 0x7D84);
    EXPECT_EQ(rx.receive_options, 0x01);
    ASSERT_EQ(rx.payload.size, 6u);
    EXPECT_EQ(std::vector<uint8_t>(rx.payload.data, rx.payload.data + rx.payload.size),
              (std::vector<uint8_t>{'R', 'x', 'D', 'a', 't', 'a'}));
}

TEST(ReceivePacket, PayloadIsZeroCopyViewIntoFrame) {
    ReceivePacket rx;
    ASSERT_EQ(parse_receive_packet(view(kDigiRxExample), rx), Status::Ok);
    EXPECT_EQ(rx.payload.data, kDigiRxExample.data() + kFrameOverhead - 1 + kReceiveHeaderSize);
}

TEST(ReceivePacket, EmptyRfDataGivesEmptyPayload) {
    const auto frame = make_receive_frame({});
    ReceivePacket rx;
    ASSERT_EQ(parse_receive_packet(view(frame), rx), Status::Ok);
    EXPECT_EQ(rx.payload.size, 0u);
}

TEST(ReceivePacket, RejectsBadStartDelimiter) {
    auto frame = kDigiRxExample;
    frame[0] = 0x7F;
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::BadStartDelimiter);
}

TEST(ReceivePacket, RejectsBadChecksum) {
    auto frame = kDigiRxExample;
    frame.back() ^= 0x01;
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::BadChecksum);
}

TEST(ReceivePacket, RejectsCorruptedDataByte) {
    auto frame = kDigiRxExample;
    frame[16] ^= 0x20;  // flip a bit in the RF data
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::BadChecksum);
}

TEST(ReceivePacket, EverySingleBitFlipIsDetected) {
    for (size_t i = 0; i < kDigiRxExample.size(); ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            auto frame = kDigiRxExample;
            frame[i] = static_cast<uint8_t>(frame[i] ^ (1u << bit));
            ReceivePacket rx;
            EXPECT_NE(parse_receive_packet(view(frame), rx), Status::Ok)
                << "undetected flip at byte " << i << " bit " << bit;
        }
    }
}

TEST(ReceivePacket, EveryTruncationIsIncomplete) {
    for (size_t n = 0; n < kDigiRxExample.size(); ++n) {
        ReceivePacket rx;
        EXPECT_EQ(parse_receive_packet(ByteView(kDigiRxExample.data(), n), rx), Status::Incomplete)
            << "prefix length " << n;
    }
}

TEST(ReceivePacket, RejectsTrailingBytes) {
    auto frame = kDigiRxExample;
    frame.push_back(0x00);
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::LengthMismatch);
}

TEST(ReceivePacket, RejectsLengthFieldLargerThanData) {
    auto frame = kDigiRxExample;
    frame[2] = static_cast<uint8_t>(frame[2] + 1);
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::Incomplete);
}

TEST(ReceivePacket, RejectsLengthFieldSmallerThanData) {
    auto frame = kDigiRxExample;
    frame[2] = static_cast<uint8_t>(frame[2] - 1);
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::LengthMismatch);
}

TEST(ReceivePacket, RejectsZeroLength) {
    const std::vector<uint8_t> frame = {0x7E, 0x00, 0x00, 0xFF};
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::LengthMismatch);
}

TEST(ReceivePacket, RejectsOtherFrameTypes) {
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(kDigiTxExample), rx), Status::UnexpectedFrameType);

    // 0x8B Transmit Status: 7E 00 07 8B 01 FF FE 00 00 00 76
    const std::vector<uint8_t> tx_status = {0x7E, 0x00, 0x07, 0x8B, 0x01, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x76};
    EXPECT_EQ(parse_receive_packet(view(tx_status), rx), Status::UnexpectedFrameType);
}

TEST(ReceivePacket, RejectsHeaderTooShortEvenWithValidChecksum) {
    std::vector<uint8_t> frame = {0x7E, 0x00, 0x05, 0x90, 0x00, 0x13, 0xA2, 0x00, 0x00};
    test_helpers::fix_checksum(frame);
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(view(frame), rx), Status::MalformedFrame);
}

TEST(ReceivePacket, RejectsNullWithSize) {
    ReceivePacket rx;
    EXPECT_EQ(parse_receive_packet(ByteView(nullptr, 10), rx), Status::InvalidArgument);
}

TEST(ReceivePacket, OutputUntouchedOnFailure) {
    ReceivePacket rx;
    rx.source_address16 = 0x1234;
    auto frame = kDigiRxExample;
    frame.back() ^= 0xFF;
    ASSERT_NE(parse_receive_packet(view(frame), rx), Status::Ok);
    EXPECT_EQ(rx.source_address16, 0x1234);
}

// -------------------------------------------------------- round trip

TEST(Framing, BuiltFrameValidatesAndCarriesPayload) {
    std::array<uint8_t, kMaxTransmitFrameSize> out{};
    size_t size = 0;
    ASSERT_EQ(build_transmit_request(digi_tx_options(), view(kTxData0A), out.data(), out.size(), size), Status::Ok);
    ByteView data;
    ASSERT_EQ(validate_frame(ByteView(out.data(), size), data), Status::Ok);
    EXPECT_EQ(data[0], kFrameTypeTransmitRequest);
    EXPECT_EQ(std::vector<uint8_t>(data.data + kTransmitHeaderSize, data.data + data.size), kTxData0A);
}
