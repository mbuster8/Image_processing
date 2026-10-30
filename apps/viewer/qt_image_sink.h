#pragma once

#include "visual_debug/image_sink.h"

#include <QMetaType>
#include <QObject>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

Q_DECLARE_METATYPE(visual_debug::ImageMessage)

// Bridges the Server's receive thread to the UI thread. onImageReceived runs on the server thread and emits
// imageReceived; receivers connect with Qt::QueuedConnection, so their slot runs on the UI thread.
// At most kMaxPendingImages may wait for the UI, so a flood of images cannot grow Qt's event queue without
// limit. When all are taken, the server thread waits up to kMaxWaitForUi for the UI to catch up: a burst
// slows the sender down to the UI's pace instead of losing images. Only a UI stalled for longer makes the
// sender hear that the viewer is busy. The wait stays well inside the client's default reply timeout
// (500 ms).
//
// Live frames take another path. They never wait and never use those slots: each channel keeps only its
// newest waiting frame, a newer one replacing it, and the UI takes every channel's frame at once with
// takeLiveFrames(). So a sender is never slowed down, and memory holds one waiting frame per channel.
class QtImageSink : public QObject, public visual_debug::ImageSink {
    Q_OBJECT

public:
    using Clock = std::chrono::steady_clock;

    static constexpr int kMaxPendingImages = 8;
    static constexpr std::chrono::milliseconds kMaxWaitForUi{200};
    // Open live channels; a frame for a new one beyond these is refused as "too many live channels",
    // unless a channel has been idle for kLiveIdleAfter, which then makes room.
    static constexpr int kMaxLiveChannels = 8;
    // The newest frames of all open channels together; a new channel that would exceed it is refused too.
    static constexpr std::size_t kMaxLiveBytes = std::size_t{1} << 30;
    static constexpr std::chrono::seconds kLiveIdleAfter{5};

    // One channel's newest waiting frame, with how many older frames of it the sink dropped since the
    // last take.
    struct LiveFrame {
        visual_debug::ImageMessage message;
        std::uint64_t replaced = 0;
    };
    struct LiveBatch {
        std::vector<LiveFrame> frames;    // one per channel with a frame waiting, in the order they opened
        std::vector<std::string> refused; // channels refused since the last take, each named once
        std::vector<std::string> evicted; // idle channels closed to make room for new ones
    };

    explicit QtImageSink(QObject* parent = nullptr);
    // For tests: the clock that decides when a channel is idle.
    explicit QtImageSink(std::function<Clock::time_point()> now, QObject* parent = nullptr);

    visual_debug::Acceptance onImageReceived(visual_debug::ImageMessage message) override;

    // Counts the reason and returns at once. Rejections reach the UI as counts, never as one event each, so a
    // flood of them cannot grow Qt's event queue: requestsRejected is emitted only when the first count
    // appears, and the UI then takes them all.
    void onRequestRejected(std::string_view reason) override;

    // The UI calls this after taking each image. Thread-safe.
    void markHandled();

    // The rejections counted since the last take, by reason; clears them. Thread-safe.
    std::map<std::string, int> takeRejections();

    // Everything live that happened since the last take; clears it. Thread-safe.
    LiveBatch takeLiveFrames();
    // Frees the channel's place and drops its waiting frame. A later frame opens it again. Thread-safe.
    void closeLiveChannel(const std::string& name);

signals:
    void imageReceived(visual_debug::ImageMessage message);
    void requestsRejected(); // connect queued, then call takeRejections()
    // Emitted when the first live news waits after a take, however many follow: connect queued, then call
    // takeLiveFrames().
    void liveFramesWaiting();

private:
    struct LiveChannel {
        std::string name;
        Clock::time_point lastArrival;
        std::size_t newestBytes = 0;
        std::optional<LiveFrame> waiting;
    };

    visual_debug::Acceptance takeLiveFrame(visual_debug::ImageMessage message); // with m_mutex held
    bool roomForNewChannel(std::size_t bytes) const;
    bool evictLongestIdle(Clock::time_point now);
    bool noteLiveNews(); // true when the UI must be told

    std::function<Clock::time_point()> m_now;
    std::mutex m_mutex;
    std::condition_variable m_slotFreed;
    int m_pending = 0;
    std::map<std::string, int> m_rejections; // reasons are a handful of fixed texts
    std::vector<LiveChannel> m_liveChannels; // in the order they opened
    LiveBatch m_liveNews;                    // refused and evicted names; frames stay with their channels
    bool m_liveNewsSignalled = false;
};
