#pragma once

#include "visual_debug/image_sink.h"

#include <QMetaType>
#include <QObject>

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

Q_DECLARE_METATYPE(visual_debug::ImageMessage)

// Bridges the Server's receive thread to the UI thread. onImageReceived runs on the server thread and emits
// imageReceived; receivers connect with Qt::QueuedConnection, so their slot runs on the UI thread.
// At most kMaxPendingImages may wait for the UI, so a flood of images cannot grow Qt's event queue without
// limit. When all are taken, the server thread waits up to kMaxWaitForUi for the UI to catch up: a burst
// slows the sender down to the UI's pace instead of losing images. Only a UI stalled for longer makes the
// sender hear that the viewer is busy. The wait stays well inside the client's default reply timeout
// (500 ms).
class QtImageSink : public QObject, public visual_debug::ImageSink {
    Q_OBJECT

public:
    static constexpr int kMaxPendingImages = 8;
    static constexpr std::chrono::milliseconds kMaxWaitForUi{200};

    explicit QtImageSink(QObject* parent = nullptr);

    visual_debug::Acceptance onImageReceived(visual_debug::ImageMessage message) override;

    // Counts the reason and returns at once. Rejections reach the UI as counts, never as one event each, so a
    // flood of them cannot grow Qt's event queue: requestsRejected is emitted only when the first count
    // appears, and the UI then takes them all.
    void onRequestRejected(std::string_view reason) override;

    // The UI calls this after taking each image. Thread-safe.
    void markHandled();

    // The rejections counted since the last take, by reason; clears them. Thread-safe.
    std::map<std::string, int> takeRejections();

signals:
    void imageReceived(visual_debug::ImageMessage message);
    void requestsRejected(); // connect queued, then call takeRejections()

private:
    std::mutex m_mutex;
    std::condition_variable m_slotFreed;
    int m_pending = 0;
    std::map<std::string, int> m_rejections; // reasons are a handful of fixed texts
};
