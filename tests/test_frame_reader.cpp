#include <gtest/gtest.h>

#include <vector>

#include "cansat_comms/xbee_frame_reader.hpp"
#include "test_helpers.hpp"

using namespace cansat;
using namespace cansat::xbee;
using test_helpers::make_receive_frame;

namespace {

using Result = FrameReader::Result;

// Pushes every byte; returns the frames that completed.
std::vector<std::vector<uint8_t>> feed(FrameReader& reader, const std::vector<uint8_t>& bytes) {
    std::vector<std::vector<uint8_t>> frames;
    for (uint8_t b : bytes) {
        if (reader.push(b) == Result::FrameReady) {
            const ByteView f = reader.frame();
            frames.emplace_back(f.data, f.data + f.size);
        }
    }
    return frames;
}

}  // namespace

TEST(FrameReader, AssemblesFrameByteByByte) {
    const auto frame = make_receive_frame({0x02, 0x05, 0x01});
    FrameReader reader;
    for (size_t i = 0; i + 1 < frame.size(); ++i) {
        EXPECT_EQ(reader.push(frame[i]), Result::NeedMore) << "byte " << i;
    }
    ASSERT_EQ(reader.push(frame.back()), Result::FrameReady);
    const ByteView got = reader.frame();
    EXPECT_EQ(std::vector<uint8_t>(got.data, got.data + got.size), frame);
}

TEST(FrameReader, SkipsNoiseBeforeStartDelimiter) {
    const auto frame = make_receive_frame({0x03, 0x01});
    std::vector<uint8_t> stream = {0x00, 0x13, 0xFF, 0x42};
    stream.insert(stream.end(), frame.begin(), frame.end());
    FrameReader reader;
    const auto frames = feed(reader, stream);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0], frame);
    EXPECT_EQ(reader.error_count(), 0u);
}

TEST(FrameReader, HandlesBackToBackFrames) {
    const auto a = make_receive_frame({0x02, 0x01, 0x01});
    const auto b = make_receive_frame({0x03, 0x00});
    std::vector<uint8_t> stream = a;
    stream.insert(stream.end(), b.begin(), b.end());
    FrameReader reader;
    const auto frames = feed(reader, stream);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0], a);
    EXPECT_EQ(frames[1], b);
}

TEST(FrameReader, DropsCorruptFrameAndResyncs) {
    auto bad = make_receive_frame({0x02, 0x01, 0x01});
    bad.back() ^= 0x55;  // corrupt checksum
    const auto good = make_receive_frame({0x03, 0x01});
    std::vector<uint8_t> stream = bad;
    stream.insert(stream.end(), good.begin(), good.end());

    FrameReader reader;
    const auto frames = feed(reader, stream);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0], good);
    EXPECT_EQ(reader.error_count(), 1u);
    EXPECT_EQ(reader.last_error(), Status::BadChecksum);
}

TEST(FrameReader, RejectsOversizedLengthWithoutOverflow) {
    // Length 0xFFFF would overflow a naive fixed buffer.
    FrameReader reader;
    EXPECT_EQ(reader.push(0x7E), Result::NeedMore);
    EXPECT_EQ(reader.push(0xFF), Result::NeedMore);
    EXPECT_EQ(reader.push(0xFF), Result::Error);
    EXPECT_EQ(reader.last_error(), Status::PayloadTooLarge);

    // Still works afterwards.
    const auto good = make_receive_frame({0x03, 0x00});
    EXPECT_EQ(feed(reader, good).size(), 1u);
}

TEST(FrameReader, RejectsZeroLength) {
    FrameReader reader;
    reader.push(0x7E);
    reader.push(0x00);
    EXPECT_EQ(reader.push(0x00), Result::Error);
    EXPECT_EQ(reader.last_error(), Status::LengthMismatch);
}

TEST(FrameReader, AcceptsLargestAllowedFrame) {
    const std::vector<uint8_t> rf(kMaxRfPayload, 0xA5);
    const auto frame = make_receive_frame(rf);
    ASSERT_EQ(frame.size(), FrameReader::kCapacity);
    FrameReader reader;
    EXPECT_EQ(feed(reader, frame).size(), 1u);
}

TEST(FrameReader, IncompleteFrameNeverReported) {
    const auto frame = make_receive_frame({0x02, 0x01, 0x01});
    FrameReader reader;
    const std::vector<uint8_t> partial(frame.begin(), frame.end() - 1);
    EXPECT_TRUE(feed(reader, partial).empty());
    EXPECT_EQ(reader.frame().size, 0u);
}

TEST(FrameReader, ResetDiscardsPartialFrame) {
    const auto frame = make_receive_frame({0x03, 0x01});
    FrameReader reader;
    feed(reader, std::vector<uint8_t>(frame.begin(), frame.begin() + 5));
    reader.reset();
    EXPECT_EQ(feed(reader, frame).size(), 1u);
}
