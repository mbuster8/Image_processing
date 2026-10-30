#include "live_sender.h"

#include "test_support.h"

#include "visual_debug/client.h"
#include "visual_debug/endpoint.h"
#include "visual_debug/server.h"
#include "visual_debug/wire_format.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using namespace std::chrono_literals;
using live_sender::Arguments;
using live_sender::CameraSource;
using live_sender::FileSource;
using live_sender::PatternSource;
using visual_debug::SendResult;

std::optional<Arguments> parse(const std::vector<std::string>& words) {
    std::vector<std::string> storage{"live_sender"};
    storage.insert(storage.end(), words.begin(), words.end());
    std::vector<char*> argv;
    for (std::string& word : storage) {
        argv.push_back(word.data());
    }
    return live_sender::parseArguments(static_cast<int>(argv.size()), argv.data());
}

std::string joined(const std::vector<std::string>& words) {
    std::string text;
    for (const std::string& word : words) {
        text += word + ' ';
    }
    return text;
}

// An endless source of pattern frames that counts how many were read.
std::function<std::optional<cv::Mat>()> patternSource(int& read) {
    return [&read]() -> std::optional<cv::Mat> { return live_sender::patternFrame(read++, {64, 48}); };
}

// Records every call and answers each channel with the result it is given.
class RecordingSender : public visual_debug::ImageSender {
public:
    struct Call {
        std::string name;
        bool live;
        int type;
    };

    explicit RecordingSender(std::function<SendResult(std::string_view)> answer =
                                 [](std::string_view) { return SendResult::Delivered; })
        : m_answer(std::move(answer)) {}

    SendResult send(const cv::Mat& image, std::string_view name) override {
        calls.push_back({std::string(name), false, image.type()});
        return m_answer(name);
    }

    SendResult sendLive(const cv::Mat& frame, std::string_view channel) override {
        calls.push_back({std::string(channel), true, frame.type()});
        return m_answer(channel);
    }

    std::vector<Call> calls;

private:
    std::function<SendResult(std::string_view)> m_answer;
};

TEST(LiveSender, ParsesEverySource) {
    const auto none = parse({});
    ASSERT_TRUE(none);
    ASSERT_TRUE(std::holds_alternative<CameraSource>(none->source));
    EXPECT_EQ(std::get<CameraSource>(none->source).index, 0);
    EXPECT_EQ(none->frames, std::nullopt);
    EXPECT_EQ(none->endpoint, visual_debug::defaultEndpoint());
    EXPECT_FALSE(none->listCameras);

    const auto camera = parse({"--camera", "2", "--frames", "5", "--endpoint", "tcp://127.0.0.1:7200"});
    ASSERT_TRUE(camera);
    ASSERT_TRUE(std::holds_alternative<CameraSource>(camera->source));
    EXPECT_EQ(std::get<CameraSource>(camera->source).index, 2);
    EXPECT_EQ(camera->frames, 5);
    EXPECT_EQ(camera->endpoint, "tcp://127.0.0.1:7200");

    const auto file = parse({"clip.mov"});
    ASSERT_TRUE(file);
    ASSERT_TRUE(std::holds_alternative<FileSource>(file->source));
    EXPECT_EQ(std::get<FileSource>(file->source).path, "clip.mov");

    const auto pattern = parse({"--pattern"});
    ASSERT_TRUE(pattern);
    ASSERT_TRUE(std::holds_alternative<PatternSource>(pattern->source));
    EXPECT_EQ(std::get<PatternSource>(pattern->source).size, cv::Size(640, 480));
    EXPECT_EQ(std::get<PatternSource>(pattern->source).fps, 30.0);

    const auto sized = parse({"--fps", "12.5", "--pattern", "--size", "1920x1080"});
    ASSERT_TRUE(sized);
    ASSERT_TRUE(std::holds_alternative<PatternSource>(sized->source));
    EXPECT_EQ(std::get<PatternSource>(sized->source).size, cv::Size(1920, 1080));
    EXPECT_EQ(std::get<PatternSource>(sized->source).fps, 12.5);

    const auto list = parse({"--list-cameras"});
    ASSERT_TRUE(list);
    EXPECT_TRUE(list->listCameras);

    const std::vector<std::vector<std::string>> rejected{{"--camera"},
                                                         {"--camera", "x"},
                                                         {"--camera", "-1"},
                                                         {"--camera", "1x"},
                                                         {"--frames", "0"},
                                                         {"--frames", "-3"},
                                                         {"--frames"},
                                                         {"--pattern", "--size", "0x5"},
                                                         {"--pattern", "--size", "640"},
                                                         {"--pattern", "--fps", "0"},
                                                         {"--pattern", "--fps", "fast"},
                                                         {"--pattern", "--fps", "0.05"},
                                                         {"--pattern", "--fps", "5000"},
                                                         {"--pattern", "--size", "40000x10"},
                                                         {"--fps", "30"},
                                                         {"--size", "320x240"},
                                                         {"--camera", "1", "--pattern"},
                                                         {"a.mov", "b.mov"},
                                                         {"--pattern", "clip.mov"},
                                                         {"--camera", "0", "--camera", "1"},
                                                         {"--verbose"},
                                                         {"--endpoint"},
                                                         {"--list-cameras", "--pattern"}};
    for (const auto& words : rejected) {
        EXPECT_FALSE(parse(words)) << joined(words);
    }
}

TEST(LiveSender, PatternMovesAndIsSendable) {
    const cv::Mat first = live_sender::patternFrame(0, {640, 480});
    const cv::Mat second = live_sender::patternFrame(1, {640, 480});

    EXPECT_EQ(first.type(), CV_8UC3);
    EXPECT_EQ(first.size(), cv::Size(640, 480));
    EXPECT_TRUE(visual_debug::isSupportedImage(first));
    EXPECT_GT(cv::norm(first, second, cv::NORM_INF), 0.0);
    EXPECT_EQ(live_sender::patternFrame(5, {320, 200}).size(), cv::Size(320, 200));
    EXPECT_EQ(live_sender::patternFrame(3, {1, 1}).size(), cv::Size(1, 1));
}

TEST(LiveSender, EdgesAreSingleChannelOfTheSameSize) {
    const cv::Mat frame = live_sender::patternFrame(0, {320, 240});

    const cv::Mat edges = live_sender::edgesOf(frame);

    EXPECT_EQ(edges.type(), CV_8UC1);
    EXPECT_EQ(edges.size(), frame.size());
    EXPECT_GT(cv::countNonZero(edges), 0);
    EXPECT_EQ(live_sender::edgesOf(cv::Mat(10, 12, CV_8UC1, cv::Scalar(7))).type(), CV_8UC1);
    EXPECT_EQ(live_sender::edgesOf(cv::Mat(10, 12, CV_8UC4, cv::Scalar(1, 2, 3, 4))).type(), CV_8UC1);
}

TEST(LiveSender, SendsBothChannelsPerFrame) {
    RecordingSender sender;
    int read = 0;

    const live_sender::Tally tally = live_sender::run(patternSource(read), sender, 30, [] { return true; });

    ASSERT_EQ(sender.calls.size(), 60u);
    for (std::size_t i = 0; i < sender.calls.size(); ++i) {
        const bool camera = i % 2 == 0;
        EXPECT_EQ(sender.calls[i].name, camera ? "camera" : "edges") << i;
        EXPECT_TRUE(sender.calls[i].live) << i;
        EXPECT_EQ(sender.calls[i].type, camera ? CV_8UC3 : CV_8UC1) << i;
    }
    EXPECT_EQ(read, 30); // nothing is read beyond --frames
    EXPECT_EQ(tally.camera.sent, 30);
    EXPECT_EQ(tally.camera.delivered, 30);
    EXPECT_TRUE(tally.camera.failed.empty());
    EXPECT_EQ(tally.edges.sent, 30);
    EXPECT_EQ(tally.edges.delivered, 30);
    EXPECT_TRUE(tally.edges.failed.empty());
}

TEST(LiveSender, StopsWhenWaitSaysSo) {
    RecordingSender sender;
    int read = 0;
    int waits = 0;

    const live_sender::Tally tally =
        live_sender::run(patternSource(read), sender, std::nullopt, [&waits] { return ++waits <= 3; });

    EXPECT_EQ(waits, 4);
    EXPECT_EQ(read, 3);
    EXPECT_EQ(tally.camera.sent, 3);
    EXPECT_EQ(tally.edges.sent, 3);
}

TEST(LiveSender, StopsAtTheEndOfTheSource) {
    RecordingSender sender;
    int read = 0;
    const auto fiveFrames = [&read]() -> std::optional<cv::Mat> {
        if (read == 5) {
            return std::nullopt;
        }
        return live_sender::patternFrame(read++, {64, 48});
    };

    const live_sender::Tally tally = live_sender::run(fiveFrames, sender, 30, [] { return true; });

    EXPECT_EQ(tally.camera.sent, 5);
    EXPECT_EQ(tally.edges.sent, 5);
}

TEST(LiveSender, CountsFailuresByResult) {
    RecordingSender sender([](std::string_view channel) {
        return channel == "edges" ? SendResult::ViewerUnavailable : SendResult::Delivered;
    });
    int read = 0;

    const live_sender::Tally tally = live_sender::run(patternSource(read), sender, 4, [] { return true; });

    EXPECT_EQ(tally.camera.delivered, 4);
    EXPECT_EQ(tally.edges.sent, 4);
    EXPECT_EQ(tally.edges.delivered, 0);
    EXPECT_EQ(tally.edges.failed, (std::map<SendResult, int>{{SendResult::ViewerUnavailable, 4}}));
}

TEST(LiveSender, DeliversThirtyFramesPerChannelToAServer) {
    test_support::CollectingSink sink;
    visual_debug::Server server(sink, {.endpoint = "tcp://127.0.0.1:*"});
    ASSERT_EQ(server.start(), std::nullopt);
    visual_debug::Client client({.endpoint = server.boundEndpoint()});
    int read = 0;

    const live_sender::Tally tally = live_sender::run(patternSource(read), client, 30, [] { return true; });

    EXPECT_EQ(tally.camera.delivered, 30);
    EXPECT_EQ(tally.edges.delivered, 30);
    ASSERT_TRUE(sink.waitForCount(60, 5s));
    int camera = 0;
    int edges = 0;
    for (const visual_debug::ImageMessage& message : sink.messages()) {
        EXPECT_TRUE(message.live);
        EXPECT_TRUE(message.sentAt.has_value());
        camera += message.name == "camera" ? 1 : 0;
        edges += message.name == "edges" ? 1 : 0;
    }
    EXPECT_EQ(camera, 30);
    EXPECT_EQ(edges, 30);
}

} // namespace
