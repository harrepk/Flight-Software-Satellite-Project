#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "cansat_comms/payload.hpp"
#include "test_helpers.hpp"

using namespace cansat;
using namespace cansat::payload;
using test_helpers::view;

// ---------------------------------------------------- OutTelemetry serialize

TEST(SerializeOutTelemetry, KnownBytesBigEndian) {
    const OutTelemetry msg{0x12345678u, 0xABCDu};
    std::array<uint8_t, kMaxPayloadSize> out{};
    size_t written = 0;
    ASSERT_EQ(serialize(msg, out.data(), out.size(), written), Status::Ok);
    ASSERT_EQ(written, kOutTelemetrySize);
    const std::array<uint8_t, 7> expected = {0x01, 0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD};
    EXPECT_EQ(out, expected);
}

TEST(SerializeOutTelemetry, RealisticTimestamp) {
    // 2026-10-13 00:00:00 UTC = 1791849600 = 0x6ACD7480; mechanisms 0 and 3 deployed.
    const OutTelemetry msg{1791849600u, 0b0000'0000'0000'1001};
    std::array<uint8_t, kMaxPayloadSize> out{};
    size_t written = 0;
    ASSERT_EQ(serialize(msg, out.data(), out.size(), written), Status::Ok);
    const std::array<uint8_t, 7> expected = {0x01, 0x6A, 0xCD, 0x74, 0x80, 0x00, 0x09};
    EXPECT_EQ(out, expected);
}

TEST(SerializeOutTelemetry, ExtremeValues) {
    std::array<uint8_t, kMaxPayloadSize> out{};
    size_t written = 0;
    ASSERT_EQ(serialize(OutTelemetry{0u, 0u}, out.data(), out.size(), written), Status::Ok);
    EXPECT_EQ(out, (std::array<uint8_t, 7>{0x01, 0, 0, 0, 0, 0, 0}));
    ASSERT_EQ(serialize(OutTelemetry{0xFFFFFFFFu, 0xFFFFu}, out.data(), out.size(), written), Status::Ok);
    EXPECT_EQ(out, (std::array<uint8_t, 7>{0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));
}

TEST(SerializeOutTelemetry, IsDeterministic) {
    const OutTelemetry msg{42u, 7u};
    std::array<uint8_t, kMaxPayloadSize> a{}, b{};
    size_t wa = 0, wb = 0;
    serialize(msg, a.data(), a.size(), wa);
    serialize(msg, b.data(), b.size(), wb);
    EXPECT_EQ(a, b);
}

TEST(SerializeOutTelemetry, BufferTooSmallWritesNothing) {
    std::vector<uint8_t> out(kOutTelemetrySize, 0xCC);
    size_t written = 99;
    EXPECT_EQ(serialize(OutTelemetry{1u, 1u}, out.data(), kOutTelemetrySize - 1, written),
              Status::BufferTooSmall);
    EXPECT_EQ(written, 0u);
    for (uint8_t b : out) EXPECT_EQ(b, 0xCC);
    EXPECT_EQ(serialize(OutTelemetry{1u, 1u}, nullptr, 100, written), Status::BufferTooSmall);
}

TEST(OutTelemetryRoundTrip, AlsoMatchesKnownBytes) {
    const std::vector<uint8_t> bytes = {0x01, 0xDE, 0xAD, 0xBE, 0xEF, 0x80, 0x01};
    OutTelemetry msg{};
    ASSERT_EQ(deserialize(view(bytes), msg), Status::Ok);
    EXPECT_EQ(msg.seconds_since_epoch, 0xDEADBEEFu);
    EXPECT_EQ(msg.mechanisms_deployed_flags, 0x8001u);

    std::array<uint8_t, kMaxPayloadSize> out{};
    size_t written = 0;
    ASSERT_EQ(serialize(msg, out.data(), out.size(), written), Status::Ok);
    EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.begin() + static_cast<long>(written)), bytes);
}

// ------------------------------------------------- CommandMechanism

TEST(DeserializeCommandMechanism, KnownBytes) {
    const std::vector<uint8_t> bytes = {0x02, 0x04, 0xB4};
    CommandMechanism cmd{};
    ASSERT_EQ(deserialize(view(bytes), cmd), Status::Ok);
    EXPECT_EQ(cmd.mechanism_id, 0x04);
    EXPECT_EQ(cmd.value, 0xB4);
}

TEST(DeserializeCommandMechanism, FullValueRange) {
    for (unsigned id : {0u, 1u, 127u, 255u}) {
        for (unsigned val : {0u, 128u, 255u}) {
            const std::vector<uint8_t> bytes = {0x02, static_cast<uint8_t>(id), static_cast<uint8_t>(val)};
            CommandMechanism cmd{};
            ASSERT_EQ(deserialize(view(bytes), cmd), Status::Ok);
            EXPECT_EQ(cmd.mechanism_id, id);
            EXPECT_EQ(cmd.value, val);
        }
    }
}

TEST(DeserializeCommandMechanism, RejectsShortPayload) {
    CommandMechanism cmd{};
    EXPECT_EQ(deserialize(view({0x02, 0x04}), cmd), Status::Incomplete);
    EXPECT_EQ(deserialize(view({0x02}), cmd), Status::Incomplete);
}

TEST(DeserializeCommandMechanism, RejectsLongPayload) {
    CommandMechanism cmd{};
    EXPECT_EQ(deserialize(view({0x02, 0x04, 0x01, 0x00}), cmd), Status::BadPayloadLength);
}

TEST(DeserializeCommandMechanism, RejectsWrongMessageType) {
    CommandMechanism cmd{};
    EXPECT_EQ(deserialize(view({0x03, 0x01, 0x00}), cmd), Status::UnexpectedMessageType);
}

TEST(DeserializeCommandMechanism, OutputUntouchedOnFailure) {
    CommandMechanism cmd{9, 9};
    ASSERT_NE(deserialize(view({0x02, 0x04}), cmd), Status::Ok);
    EXPECT_EQ(cmd.mechanism_id, 9);
    EXPECT_EQ(cmd.value, 9);
}

// ------------------------------------------------- CommandTelemetry

TEST(DeserializeCommandTelemetry, OnAndOff) {
    CommandTelemetry cmd{false};
    ASSERT_EQ(deserialize(view({0x03, 0x01}), cmd), Status::Ok);
    EXPECT_TRUE(cmd.is_on);
    ASSERT_EQ(deserialize(view({0x03, 0x00}), cmd), Status::Ok);
    EXPECT_FALSE(cmd.is_on);
}

TEST(DeserializeCommandTelemetry, RejectsNonBooleanValue) {
    CommandTelemetry cmd{true};
    for (unsigned v : {0x02u, 0x7Fu, 0xFFu}) {
        EXPECT_EQ(deserialize(view({0x03, static_cast<uint8_t>(v)}), cmd), Status::InvalidFieldValue);
    }
    EXPECT_TRUE(cmd.is_on) << "output must be untouched on failure";
}

TEST(DeserializeCommandTelemetry, RejectsWrongLength) {
    CommandTelemetry cmd{};
    EXPECT_EQ(deserialize(view({0x03}), cmd), Status::Incomplete);
    EXPECT_EQ(deserialize(view({0x03, 0x01, 0x00}), cmd), Status::BadPayloadLength);
}

TEST(DeserializeCommandTelemetry, RejectsWrongMessageType) {
    CommandTelemetry cmd{};
    EXPECT_EQ(deserialize(view({0x02, 0x01}), cmd), Status::UnexpectedMessageType);
}

// ------------------------------------------------- type identification

TEST(PeekMessageType, IdentifiesAllTypes) {
    MessageType t{};
    ASSERT_EQ(peek_message_type(view({0x01}), t), Status::Ok);
    EXPECT_EQ(t, MessageType::OutTelemetry);
    ASSERT_EQ(peek_message_type(view({0x02}), t), Status::Ok);
    EXPECT_EQ(t, MessageType::CommandMechanism);
    ASSERT_EQ(peek_message_type(view({0x03}), t), Status::Ok);
    EXPECT_EQ(t, MessageType::CommandTelemetry);
}

TEST(PeekMessageType, RejectsUnknownAndEmpty) {
    MessageType t{};
    for (unsigned id : {0x00u, 0x04u, 0x7Eu, 0xFFu}) {
        EXPECT_EQ(peek_message_type(view({static_cast<uint8_t>(id)}), t), Status::UnknownMessageType) << id;
    }
    EXPECT_EQ(peek_message_type(ByteView(), t), Status::Incomplete);
    EXPECT_EQ(peek_message_type(ByteView(nullptr, 4), t), Status::InvalidArgument);
}

TEST(DecodeGroundCommand, DispatchesByType) {
    GroundCommand cmd;
    ASSERT_EQ(decode_ground_command(view({0x02, 0x01, 0x5A}), cmd), Status::Ok);
    EXPECT_EQ(cmd.type, MessageType::CommandMechanism);
    EXPECT_EQ(cmd.mechanism.mechanism_id, 0x01);
    EXPECT_EQ(cmd.mechanism.value, 0x5A);

    ASSERT_EQ(decode_ground_command(view({0x03, 0x01}), cmd), Status::Ok);
    EXPECT_EQ(cmd.type, MessageType::CommandTelemetry);
    EXPECT_TRUE(cmd.telemetry.is_on);
}

TEST(DecodeGroundCommand, RejectsDownlinkAndGarbage) {
    GroundCommand cmd;
    EXPECT_EQ(decode_ground_command(view({0x01, 0, 0, 0, 0, 0, 0}), cmd), Status::UnexpectedMessageType);
    EXPECT_EQ(decode_ground_command(view({0x99, 0x01}), cmd), Status::UnknownMessageType);
    EXPECT_EQ(decode_ground_command(ByteView(), cmd), Status::Incomplete);
    EXPECT_EQ(decode_ground_command(view({0x03, 0x05}), cmd), Status::InvalidFieldValue);
}
