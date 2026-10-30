#include "viewer_core/live_channels.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;
using viewer_core::LiveChannel;
using viewer_core::LiveChannels;
using viewer_core::LiveFrameIn;
using viewer_core::SentTime;

// Any steady time will do: the class only ever compares and subtracts them.
const LiveChannels::SteadyTime kStart = LiveChannels::SteadyTime{} + 1000s;

LiveFrameIn frameFor(const std::string& channel, int value = 0, std::uint64_t replaced = 0) {
    return {.channel = channel, .image = cv::Mat(2, 2, CV_8UC1, cv::Scalar(value)), .replaced = replaced};
}

int valueOf(const cv::Mat& image) { return image.at<unsigned char>(0, 0); }

std::vector<std::string> namesOf(const LiveChannels& channels) {
    std::vector<std::string> names;
    for (const LiveChannel& channel : channels.channels()) {
        names.push_back(channel.name);
    }
    return names;
}

TEST(LiveChannels, FirstFrameStartsAChannel) {
    LiveChannels channels;

    const LiveChannels::ReceiveResult first = channels.receive(frameFor("camera", 7), kStart);

    EXPECT_TRUE(first.started);
    EXPECT_TRUE(first.shownChanged);
    const LiveChannel* camera = channels.find("camera");
    ASSERT_NE(camera, nullptr);
    EXPECT_EQ(valueOf(camera->shown), 7);
    EXPECT_TRUE(camera->newest.empty());
    EXPECT_EQ(camera->received, 1u);
    EXPECT_FALSE(camera->idle);
    EXPECT_FALSE(camera->frozen);
    const LiveChannels::ReceiveResult second = channels.receive(frameFor("camera", 8), kStart + 10ms);
    EXPECT_FALSE(second.started);
    EXPECT_TRUE(second.shownChanged);
    EXPECT_EQ(valueOf(channels.find("camera")->shown), 8);
    EXPECT_EQ(channels.find("nobody"), nullptr);
}

TEST(LiveChannels, KeepsStartOrder) {
    LiveChannels channels;
    channels.receive(frameFor("edges"), kStart);
    channels.receive(frameFor("camera"), kStart);
    channels.receive(frameFor("mask"), kStart);
    channels.receive(frameFor("camera"), kStart + 10ms);
    channels.updateIdle(kStart + 5s);
    channels.receive(frameFor("mask"), kStart + 6s);

    EXPECT_EQ(namesOf(channels), (std::vector<std::string>{"edges", "camera", "mask"}));
}

TEST(LiveChannels, ArrivalRateCountsReplacedFrames) {
    LiveChannels channels;
    LiveChannels::SteadyTime now = kStart;

    for (int take = 0; take < 30; ++take) {
        now = kStart + take * 33ms;
        channels.receive(frameFor("camera", take, 1), now);
        channels.markDrawn("camera", now);
    }

    const viewer_core::LiveStats stats = channels.stats("camera", now);
    EXPECT_EQ(stats.arrivedPerSecond, 60);
    EXPECT_EQ(stats.drawnPerSecond, 30);
    const LiveChannel* camera = channels.find("camera");
    EXPECT_EQ(camera->received, 30u);
    EXPECT_EQ(camera->dropped, 30u);
    EXPECT_EQ(camera->drawn, 30u);
}

TEST(LiveChannels, RatesFallToZeroAfterTheWindow) {
    LiveChannels channels;
    channels.receive(frameFor("camera", 0, 2), kStart);
    channels.markDrawn("camera", kStart);

    EXPECT_EQ(channels.stats("camera", kStart + 999ms).arrivedPerSecond, 3);
    EXPECT_EQ(channels.stats("camera", kStart + 999ms).drawnPerSecond, 1);
    EXPECT_EQ(channels.stats("camera", kStart + 1s).arrivedPerSecond, 0);
    EXPECT_EQ(channels.stats("camera", kStart + 1s).drawnPerSecond, 0);
    EXPECT_EQ(channels.stats("nobody", kStart).arrivedPerSecond, 0);
}

TEST(LiveChannels, GoesIdleAfterFiveSecondsAndBackOnAFrame) {
    LiveChannels channels;
    channels.receive(frameFor("camera"), kStart);
    channels.receive(frameFor("edges"), kStart + 2s);

    EXPECT_TRUE(channels.updateIdle(kStart + 4999ms).empty());
    EXPECT_FALSE(channels.find("camera")->idle);
    EXPECT_EQ(channels.updateIdle(kStart + 5s), (std::vector<std::string>{"camera"}));
    EXPECT_TRUE(channels.find("camera")->idle);
    EXPECT_FALSE(channels.find("edges")->idle);
    EXPECT_TRUE(channels.updateIdle(kStart + 6s).empty());

    const LiveChannels::ReceiveResult back = channels.receive(frameFor("camera"), kStart + 7s);

    EXPECT_TRUE(back.started);
    EXPECT_FALSE(channels.find("camera")->idle);
    EXPECT_EQ(channels.updateIdle(kStart + 7s), (std::vector<std::string>{"edges"}));
}

TEST(LiveChannels, FreezeKeepsTheShownFrameWhileCounting) {
    LiveChannels channels;
    const auto receivedAt = std::chrono::system_clock::time_point{1793366602535ms};
    channels.receive(frameFor("camera", 1), kStart);

    EXPECT_FALSE(channels.setFrozen("camera", true));
    const LiveChannels::ReceiveResult whileFrozen = channels.receive(frameFor("camera", 2, 3), kStart + 10ms);
    channels.receive({.channel = "camera",
                      .image = cv::Mat(2, 2, CV_8UC1, cv::Scalar(3)),
                      .receivedAt = receivedAt,
                      .sentAt = SentTime{1793366602530ms}},
                     kStart + 20ms);

    EXPECT_FALSE(whileFrozen.shownChanged);
    const LiveChannel* camera = channels.find("camera");
    EXPECT_TRUE(camera->frozen);
    EXPECT_EQ(valueOf(camera->shown), 1);
    EXPECT_EQ(valueOf(camera->newest), 3);
    EXPECT_EQ(camera->received, 3u);
    EXPECT_EQ(camera->dropped, 3u);
    EXPECT_EQ(channels.stats("camera", kStart + 20ms).arrivedPerSecond, 6);
    EXPECT_EQ(channels.stats("camera", kStart + 20ms).age, std::nullopt);

    EXPECT_TRUE(channels.setFrozen("camera", false));

    camera = channels.find("camera");
    EXPECT_FALSE(camera->frozen);
    EXPECT_EQ(valueOf(camera->shown), 3);
    EXPECT_TRUE(camera->newest.empty());
    EXPECT_EQ(camera->shownReceivedAt, receivedAt);
    EXPECT_EQ(channels.stats("camera", kStart + 20ms).age, std::optional{5ms});
    EXPECT_FALSE(channels.setFrozen("camera", false));
    EXPECT_FALSE(channels.setFrozen("camera", true));
    EXPECT_FALSE(channels.setFrozen("camera", false)); // nothing arrived while frozen
    EXPECT_FALSE(channels.setFrozen("nobody", true));
}

TEST(LiveChannels, AgeComesFromSentAt) {
    LiveChannels channels;
    // The receive time's fraction of a millisecond is dropped, not rounded.
    const auto receivedAt = std::chrono::system_clock::time_point{1793366602535ms + 900us};
    channels.receive({.channel = "camera",
                      .image = cv::Mat(2, 2, CV_8UC1),
                      .receivedAt = receivedAt,
                      .sentAt = SentTime{1793366602517ms}},
                     kStart);
    channels.receive({.channel = "file", .image = cv::Mat(2, 2, CV_8UC1), .receivedAt = receivedAt}, kStart);

    EXPECT_EQ(channels.stats("camera", kStart).age, std::optional{18ms});
    EXPECT_EQ(channels.stats("file", kStart).age, std::nullopt);
    EXPECT_EQ(channels.stats("nobody", kStart).age, std::nullopt);
}

// The latest sentAt the wire format allows is past what a nanosecond system_clock can hold (libstdc++'s ends
// in 2262), so the age must be worked out in milliseconds. The asan preset turns an overflow into a failure.
TEST(LiveChannels, AgeWithTheLatestSentAtDoesNotOverflow) {
    LiveChannels channels;
    channels.receive({.channel = "camera",
                      .image = cv::Mat(2, 2, CV_8UC1),
                      .receivedAt = std::chrono::system_clock::time_point{1793366602535ms},
                      .sentAt = SentTime{253402300799999ms}},
                     kStart);

    EXPECT_EQ(channels.stats("camera", kStart).age, std::optional{1793366602535ms - 253402300799999ms});
}

TEST(LiveChannels, CloseForgetsTheChannel) {
    LiveChannels channels;
    channels.receive(frameFor("camera", 0, 4), kStart);
    channels.receive(frameFor("edges"), kStart);

    EXPECT_TRUE(channels.close("camera"));

    EXPECT_EQ(channels.find("camera"), nullptr);
    EXPECT_EQ(namesOf(channels), (std::vector<std::string>{"edges"}));
    EXPECT_FALSE(channels.close("camera"));
    EXPECT_EQ(channels.stats("camera", kStart).arrivedPerSecond, 0);
    channels.markDrawn("camera", kStart);
    EXPECT_TRUE(channels.updateIdle(kStart + 4s).empty());
    const LiveChannels::ReceiveResult reopened = channels.receive(frameFor("camera"), kStart + 4s);
    EXPECT_TRUE(reopened.started);
    EXPECT_EQ(channels.find("camera")->received, 1u);
    EXPECT_EQ(channels.find("camera")->dropped, 0u);
    EXPECT_EQ(channels.stats("camera", kStart + 4s).arrivedPerSecond, 1);
    EXPECT_EQ(namesOf(channels), (std::vector<std::string>{"edges", "camera"}));
}

} // namespace
