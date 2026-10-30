#include "qt_image_sink.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QSignalSpy>
#include <QThread>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using visual_debug::Acceptance;

visual_debug::ImageMessage smallMessage() { return {"x", cv::Mat(1, 1, CV_8UC1, cv::Scalar(0))}; }

// A live frame of `channel` holding one 32-bit value, so a test can tell frames apart.
visual_debug::ImageMessage liveFrame(const std::string& channel, int value = 0) {
    return {.name = channel, .image = cv::Mat(1, 1, CV_32SC1, cv::Scalar(value)), .live = true};
}

int valueOf(const QtImageSink::LiveFrame& frame) { return frame.message.image.at<int>(0, 0); }

std::vector<std::string> namesOf(const std::vector<QtImageSink::LiveFrame>& frames) {
    std::vector<std::string> names;
    for (const auto& frame : frames) {
        names.push_back(frame.message.name);
    }
    return names;
}

// Opens channels c0 to c<count - 1>, one frame each.
void openChannels(QtImageSink& sink, int count) {
    for (int i = 0; i < count; ++i) {
        ASSERT_EQ(sink.onImageReceived(liveFrame("c" + std::to_string(i))), Acceptance::Taken) << i;
    }
}

TEST(QtImageSink, DeliversOnReceiverThread) {
    QtImageSink sink;
    QObject receiver; // lives on this, the QApplication thread
    Qt::HANDLE slotThread = nullptr;
    QObject::connect(
        &sink, &QtImageSink::imageReceived, &receiver,
        [&](const visual_debug::ImageMessage&) { slotThread = QThread::currentThreadId(); },
        Qt::QueuedConnection);

    Acceptance accepted = Acceptance::Busy;
    std::thread serverThread([&] { accepted = sink.onImageReceived(smallMessage()); });
    serverThread.join();

    QElapsedTimer timer;
    timer.start();
    while (slotThread == nullptr && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    EXPECT_EQ(accepted, Acceptance::Taken);
    EXPECT_EQ(slotThread, QThread::currentThreadId());
}

TEST(QtImageSink, ReportsBusyWhenUiFallsBehind) {
    QtImageSink sink;

    for (int i = 0; i < QtImageSink::kMaxPendingImages; ++i) {
        EXPECT_EQ(sink.onImageReceived(smallMessage()), Acceptance::Taken) << i;
    }
    EXPECT_EQ(sink.onImageReceived(smallMessage()), Acceptance::Busy);

    sink.markHandled();
    EXPECT_EQ(sink.onImageReceived(smallMessage()), Acceptance::Taken);
}

TEST(QtImageSink, WaitsForTheUiBeforeReportingBusy) {
    QtImageSink sink;
    for (int i = 0; i < QtImageSink::kMaxPendingImages; ++i) {
        ASSERT_EQ(sink.onImageReceived(smallMessage()), Acceptance::Taken) << i;
    }

    // The UI catches up while the server thread waits: the image is accepted, not refused.
    std::thread ui([&] {
        std::this_thread::sleep_for(QtImageSink::kMaxWaitForUi / 4);
        sink.markHandled();
    });
    const Acceptance accepted = sink.onImageReceived(smallMessage());
    ui.join();

    EXPECT_EQ(accepted, Acceptance::Taken);
}

// Rejections are counted on the server thread and taken by the UI in one go, so a flood of them costs one
// queued signal, never a queue that grows with the flood.
TEST(QtImageSink, CombinesRejectionsUntilTheUiTakesThem) {
    QtImageSink sink;
    QSignalSpy rejected(&sink, &QtImageSink::requestsRejected);

    std::thread serverThread([&sink] {
        for (int i = 0; i < 3; ++i) {
            sink.onRequestRejected("viewer busy");
        }
        sink.onRequestRejected("viewer paused");
    });
    serverThread.join();
    QCoreApplication::processEvents();

    EXPECT_EQ(rejected.count(), 1);
    const std::map<std::string, int> expected{{"viewer busy", 3}, {"viewer paused", 1}};
    EXPECT_EQ(sink.takeRejections(), expected);
    EXPECT_TRUE(sink.takeRejections().empty());

    // Taken, the next rejection signals again.
    sink.onRequestRejected("viewer busy");
    EXPECT_EQ(rejected.count(), 2);
}

// --- Live frames -------------------------------------------------------------------------------------------

TEST(QtImageSink, LiveFramesNeverUseTheSlots) {
    QtImageSink sink;
    for (int i = 0; i < QtImageSink::kMaxPendingImages; ++i) {
        ASSERT_EQ(sink.onImageReceived(smallMessage()), Acceptance::Taken) << i;
    }

    QElapsedTimer timer;
    timer.start();
    EXPECT_EQ(sink.onImageReceived(liveFrame("camera")), Acceptance::Taken);
    EXPECT_LT(timer.elapsed(), QtImageSink::kMaxWaitForUi.count() / 2);
}

TEST(QtImageSink, NewestLiveFrameReplacesTheWaitingOne) {
    QtImageSink sink;
    ASSERT_EQ(sink.onImageReceived(liveFrame("camera", 1)), Acceptance::Taken);
    ASSERT_EQ(sink.onImageReceived(liveFrame("camera", 2)), Acceptance::Taken);

    const auto batch = sink.takeLiveFrames();

    ASSERT_EQ(batch.frames.size(), 1u);
    EXPECT_EQ(valueOf(batch.frames[0]), 2);
    EXPECT_EQ(batch.frames[0].replaced, 1u);
    EXPECT_TRUE(sink.takeLiveFrames().frames.empty());
}

// However fast frames arrive, the UI is told once per take, and memory holds one frame per channel.
TEST(QtImageSink, FloodCostsOneSignalAndKeepsTheNewest) {
    QtImageSink sink;
    QSignalSpy waiting(&sink, &QtImageSink::liveFramesWaiting);

    std::thread serverThread([&sink] {
        for (int i = 0; i < 1000; ++i) {
            sink.onImageReceived(liveFrame("camera", i));
        }
    });
    serverThread.join();
    QCoreApplication::processEvents();

    EXPECT_EQ(waiting.count(), 1);
    const auto batch = sink.takeLiveFrames();
    ASSERT_EQ(batch.frames.size(), 1u);
    EXPECT_EQ(valueOf(batch.frames[0]), 999);
    EXPECT_EQ(batch.frames[0].replaced, 999u);

    // Taken, the next frame signals again.
    sink.onImageReceived(liveFrame("camera"));
    EXPECT_EQ(waiting.count(), 2);
}

TEST(QtImageSink, ChannelsComeInOpeningOrder) {
    QtImageSink sink;
    sink.onImageReceived(liveFrame("b"));
    sink.onImageReceived(liveFrame("a"));

    EXPECT_EQ(namesOf(sink.takeLiveFrames().frames), (std::vector<std::string>{"b", "a"}));
}

TEST(QtImageSink, RefusesANinthActiveChannel) {
    QtImageSink sink;
    openChannels(sink, QtImageSink::kMaxLiveChannels);

    for (int attempt = 0; attempt < 3; ++attempt) {
        EXPECT_EQ(sink.onImageReceived(liveFrame("c8")), Acceptance::TooManyLiveChannels) << attempt;
    }
    // A known channel still gets through.
    EXPECT_EQ(sink.onImageReceived(liveFrame("c0")), Acceptance::Taken);

    const auto batch = sink.takeLiveFrames();
    EXPECT_EQ(batch.refused, std::vector<std::string>{"c8"});
    EXPECT_EQ(batch.frames.size(), 8u);
}

TEST(QtImageSink, ClosingFreesThePlaceAndDropsTheWaitingFrame) {
    QtImageSink sink;
    openChannels(sink, QtImageSink::kMaxLiveChannels);

    sink.closeLiveChannel("c3");

    EXPECT_EQ(sink.onImageReceived(liveFrame("c8")), Acceptance::Taken);
    const auto names = namesOf(sink.takeLiveFrames().frames);
    EXPECT_EQ(std::count(names.begin(), names.end(), "c3"), 0);
    EXPECT_EQ(names.back(), "c8");
}

// Header-only Mats over a small buffer: the sink counts bytes from the header and never reads the pixels.
TEST(QtImageSink, RefusesBeyondTheByteBudget) {
    unsigned char buffer[16] = {};
    QtImageSink sink;
    const cv::Mat sixHundredMiB(25600, 24576, CV_8UC1, buffer);
    ASSERT_EQ(sink.onImageReceived({.name = "big", .image = sixHundredMiB, .live = true}), Acceptance::Taken);

    EXPECT_EQ(sink.onImageReceived({.name = "bigger", .image = sixHundredMiB, .live = true}),
              Acceptance::TooManyLiveChannels);
    EXPECT_EQ(sink.onImageReceived(liveFrame("small")), Acceptance::Taken);
}

// The budget holds for every frame, not only a channel's first: an open channel cannot grow past it.
TEST(QtImageSink, RefusesAnOpenChannelsFrameBeyondTheByteBudget) {
    unsigned char buffer[16] = {};
    QtImageSink sink;
    const cv::Mat sixHundredMiB(25600, 24576, CV_8UC1, buffer);
    ASSERT_EQ(sink.onImageReceived({.name = "big", .image = sixHundredMiB, .live = true}), Acceptance::Taken);
    ASSERT_EQ(sink.onImageReceived(liveFrame("grows", 1)), Acceptance::Taken);

    EXPECT_EQ(sink.onImageReceived({.name = "grows", .image = sixHundredMiB, .live = true}),
              Acceptance::TooManyLiveChannels);
    // A frame the size of the one it replaces still fits.
    EXPECT_EQ(sink.onImageReceived({.name = "big", .image = sixHundredMiB, .live = true}), Acceptance::Taken);

    const auto batch = sink.takeLiveFrames();
    EXPECT_EQ(batch.refused, std::vector<std::string>{"grows"});
    ASSERT_EQ(namesOf(batch.frames), (std::vector<std::string>{"big", "grows"}));
    EXPECT_EQ(valueOf(batch.frames[1]), 1); // its earlier frame still waits
}

TEST(QtImageSink, AGrowingChannelEvictsAnIdleOne) {
    unsigned char buffer[16] = {};
    auto now = std::chrono::steady_clock::time_point{} + 1h;
    QtImageSink sink([&now] { return now; });
    const cv::Mat sixHundredMiB(25600, 24576, CV_8UC1, buffer);
    ASSERT_EQ(sink.onImageReceived({.name = "big", .image = sixHundredMiB, .live = true}), Acceptance::Taken);
    ASSERT_EQ(sink.onImageReceived(liveFrame("grows")), Acceptance::Taken);
    sink.takeLiveFrames();
    now += 6s; // both idle now: "grows" makes room by closing "big", never itself

    EXPECT_EQ(sink.onImageReceived({.name = "grows", .image = sixHundredMiB, .live = true}),
              Acceptance::Taken);

    const auto batch = sink.takeLiveFrames();
    EXPECT_EQ(batch.evicted, std::vector<std::string>{"big"});
    EXPECT_EQ(namesOf(batch.frames), std::vector<std::string>{"grows"});
}

TEST(QtImageSink, EvictsTheLongestIdleForANewChannel) {
    auto now = std::chrono::steady_clock::time_point{} + 1h;
    QtImageSink sink([&now] { return now; });
    openChannels(sink, QtImageSink::kMaxLiveChannels);
    sink.takeLiveFrames();
    // After 6 s, c0 had a frame 3 s ago and c2 to c7 just now: only c1 has been quiet for 5 s or more.
    now += 3s;
    sink.onImageReceived(liveFrame("c0"));
    now += 3s;
    for (int i = 2; i < QtImageSink::kMaxLiveChannels; ++i) {
        sink.onImageReceived(liveFrame("c" + std::to_string(i)));
    }

    EXPECT_EQ(sink.onImageReceived(liveFrame("c8")), Acceptance::Taken);

    const auto batch = sink.takeLiveFrames();
    EXPECT_EQ(batch.evicted, std::vector<std::string>{"c1"});
    EXPECT_TRUE(batch.refused.empty());
    EXPECT_EQ(namesOf(batch.frames).back(), "c8");
}

} // namespace
