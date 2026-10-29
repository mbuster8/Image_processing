#include "qt_image_sink.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QSignalSpy>
#include <QThread>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <map>
#include <string>
#include <thread>

namespace {

visual_debug::ImageMessage smallMessage() { return {"x", cv::Mat(1, 1, CV_8UC1, cv::Scalar(0))}; }

TEST(QtImageSink, DeliversOnReceiverThread) {
    QtImageSink sink;
    QObject receiver; // lives on this, the QApplication thread
    Qt::HANDLE slotThread = nullptr;
    QObject::connect(
        &sink, &QtImageSink::imageReceived, &receiver,
        [&](const visual_debug::ImageMessage&) { slotThread = QThread::currentThreadId(); },
        Qt::QueuedConnection);

    bool accepted = false;
    std::thread serverThread([&] { accepted = sink.onImageReceived(smallMessage()); });
    serverThread.join();

    QElapsedTimer timer;
    timer.start();
    while (slotThread == nullptr && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    EXPECT_TRUE(accepted);
    EXPECT_EQ(slotThread, QThread::currentThreadId());
}

TEST(QtImageSink, ReportsBusyWhenUiFallsBehind) {
    QtImageSink sink;

    for (int i = 0; i < QtImageSink::kMaxPendingImages; ++i) {
        EXPECT_TRUE(sink.onImageReceived(smallMessage())) << i;
    }
    EXPECT_FALSE(sink.onImageReceived(smallMessage()));

    sink.markHandled();
    EXPECT_TRUE(sink.onImageReceived(smallMessage()));
}

TEST(QtImageSink, WaitsForTheUiBeforeReportingBusy) {
    QtImageSink sink;
    for (int i = 0; i < QtImageSink::kMaxPendingImages; ++i) {
        ASSERT_TRUE(sink.onImageReceived(smallMessage())) << i;
    }

    // The UI catches up while the server thread waits: the image is accepted, not refused.
    std::thread ui([&] {
        std::this_thread::sleep_for(QtImageSink::kMaxWaitForUi / 4);
        sink.markHandled();
    });
    const bool accepted = sink.onImageReceived(smallMessage());
    ui.join();

    EXPECT_TRUE(accepted);
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

} // namespace
