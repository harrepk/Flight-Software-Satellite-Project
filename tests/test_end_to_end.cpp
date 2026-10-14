// Full transmit and receive paths, as described in section 2.5 of the package:
//
//   OutTelemetry -> serialize -> payload -> build 0x10 -> complete frame
//   UART bytes -> FrameReader -> parse 0x90 -> payload -> decode command

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "cansat_comms/payload.hpp"
#include "cansat_comms/xbee_frame.hpp"
#include "cansat_comms/xbee_frame_reader.hpp"
#include "test_helpers.hpp"

using namespace cansat;
using test_helpers::make_receive_frame;

TEST(EndToEnd, TelemetryToTransmitFrame) {
    const OutTelemetry msg{0x12345678u, 0x0003u};

    std::array<uint8_t, payload::kMaxPayloadSize> pl{};
    size_t pl_size = 0;
    ASSERT_EQ(payload::serialize(msg, pl.data(), pl.size(), pl_size), Status::Ok);

    std::array<uint8_t, xbee::kMaxTransmitFrameSize> frame{};
    size_t frame_size = 0;
    ASSERT_EQ(xbee::build_transmit_request(xbee::TransmitOptions{}, ByteView(pl.data(), pl_size),
                                           frame.data(), frame.size(), frame_size),
              Status::Ok);

    // Hand-computed expected frame (coordinator, 16-bit FFFE, frame ID 1).
    // Frame data sum: 10+01+FF+FE+01+12+34+56+78+00+03 = 0x326 -> checksum 0xFF-0x26 = 0xD9
    const std::vector<uint8_t> expected = {0x7E, 0x00, 0x15, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0xFF, 0xFE, 0x00, 0x00, 0x01,
                                           0x12, 0x34, 0x56, 0x78, 0x00, 0x03, 0xD9};
    EXPECT_EQ(std::vector<uint8_t>(frame.begin(), frame.begin() + static_cast<long>(frame_size)), expected);
}

TEST(EndToEnd, ReceiveStreamToCommands) {
    // Ground Station sends: deploy mechanism 2 (value 1), then telemetry ON,
    // with some line noise and one corrupted frame in between.
    const auto mech = make_receive_frame({0x02, 0x02, 0x01});
    auto corrupt = make_receive_frame({0x02, 0x07, 0xFF});
    corrupt[corrupt.size() - 2] ^= 0x01;
    const auto telem = make_receive_frame({0x03, 0x01});

    std::vector<uint8_t> uart = {0x00, 0xAA};
    uart.insert(uart.end(), mech.begin(), mech.end());
    uart.insert(uart.end(), corrupt.begin(), corrupt.end());
    uart.insert(uart.end(), telem.begin(), telem.end());

    xbee::FrameReader reader;
    std::vector<payload::GroundCommand> commands;
    for (uint8_t byte : uart) {
        if (reader.push(byte) != xbee::FrameReader::Result::FrameReady) continue;
        xbee::ReceivePacket rx;
        ASSERT_EQ(xbee::parse_receive_packet(reader.frame(), rx), Status::Ok);
        payload::GroundCommand cmd;
        ASSERT_EQ(payload::decode_ground_command(rx.payload, cmd), Status::Ok);
        commands.push_back(cmd);
    }

    ASSERT_EQ(commands.size(), 2u);
    EXPECT_EQ(commands[0].type, payload::MessageType::CommandMechanism);
    EXPECT_EQ(commands[0].mechanism.mechanism_id, 2);
    EXPECT_EQ(commands[0].mechanism.value, 1);
    EXPECT_EQ(commands[1].type, payload::MessageType::CommandTelemetry);
    EXPECT_TRUE(commands[1].telemetry.is_on);
    EXPECT_EQ(reader.error_count(), 1u);
}

TEST(EndToEnd, ValidFrameWithBadPayloadIsRejectedAtPayloadLayer) {
    // Framing is fine, but the application payload is truncated: the XBee layer
    // accepts it (it doesn't interpret payloads) and the payload layer rejects it.
    const auto frame = make_receive_frame({0x02, 0x01});
    xbee::ReceivePacket rx;
    ASSERT_EQ(xbee::parse_receive_packet(ByteView(frame.data(), frame.size()), rx), Status::Ok);
    payload::GroundCommand cmd;
    EXPECT_EQ(payload::decode_ground_command(rx.payload, cmd), Status::Incomplete);
}
