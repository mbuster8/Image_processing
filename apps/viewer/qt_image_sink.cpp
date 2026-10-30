#include "qt_image_sink.h"

#include <algorithm>
#include <iterator>
#include <utility>

QtImageSink::QtImageSink(QObject* parent) : QtImageSink(&Clock::now, parent) {}

QtImageSink::QtImageSink(std::function<Clock::time_point()> now, QObject* parent)
    : QObject(parent), m_now(std::move(now)) {
    qRegisterMetaType<visual_debug::ImageMessage>(); // queued connections copy the argument by its metatype
}

visual_debug::Acceptance QtImageSink::onImageReceived(visual_debug::ImageMessage message) {
    if (message.live) {
        visual_debug::Acceptance result = visual_debug::Acceptance::Taken;
        bool tellUi = false;
        {
            const std::lock_guard lock(m_mutex);
            result = takeLiveFrame(std::move(message));
            tellUi = noteLiveNews();
        }
        if (tellUi) {
            emit liveFramesWaiting();
        }
        return result;
    }
    {
        std::unique_lock lock(m_mutex);
        if (!m_slotFreed.wait_for(lock, kMaxWaitForUi, [this] { return m_pending < kMaxPendingImages; })) {
            return visual_debug::Acceptance::Busy;
        }
        ++m_pending;
    }
    emit imageReceived(std::move(message));
    return visual_debug::Acceptance::Taken;
}

visual_debug::Acceptance QtImageSink::takeLiveFrame(visual_debug::ImageMessage message) {
    const Clock::time_point now = m_now();
    const std::size_t bytes = message.image.total() * message.image.elemSize();
    auto channel = std::find_if(m_liveChannels.begin(), m_liveChannels.end(),
                                [&message](const LiveChannel& open) { return open.name == message.name; });
    if (channel == m_liveChannels.end()) {
        while (!roomForNewChannel(bytes)) {
            if (!evictLongestIdle(now)) {
                auto& refused = m_liveNews.refused;
                if (std::find(refused.begin(), refused.end(), message.name) == refused.end()) {
                    refused.push_back(message.name);
                }
                return visual_debug::Acceptance::TooManyLiveChannels;
            }
        }
        m_liveChannels.push_back({.name = message.name});
        channel = std::prev(m_liveChannels.end());
    }
    channel->lastArrival = now;
    channel->newestBytes = bytes;
    if (channel->waiting) {
        channel->waiting->message = std::move(message);
        ++channel->waiting->replaced;
    } else {
        channel->waiting = LiveFrame{std::move(message), 0};
    }
    return visual_debug::Acceptance::Taken;
}

bool QtImageSink::roomForNewChannel(std::size_t bytes) const {
    if (m_liveChannels.size() >= static_cast<std::size_t>(kMaxLiveChannels)) {
        return false;
    }
    std::size_t total = bytes;
    for (const LiveChannel& open : m_liveChannels) {
        total += open.newestBytes;
    }
    return total <= kMaxLiveBytes;
}

// A channel whose sender stopped keeps its place until closed, so without this a developer who restarts a
// sender under new channel names would soon be refused.
bool QtImageSink::evictLongestIdle(Clock::time_point now) {
    auto longestIdle = m_liveChannels.end();
    for (auto open = m_liveChannels.begin(); open != m_liveChannels.end(); ++open) {
        if (now - open->lastArrival >= kLiveIdleAfter &&
            (longestIdle == m_liveChannels.end() || open->lastArrival < longestIdle->lastArrival)) {
            longestIdle = open;
        }
    }
    if (longestIdle == m_liveChannels.end()) {
        return false;
    }
    m_liveNews.evicted.push_back(longestIdle->name);
    m_liveChannels.erase(longestIdle);
    return true;
}

// The UI is told once, when the first news waits; it then takes everything, however much more arrived.
bool QtImageSink::noteLiveNews() { return !std::exchange(m_liveNewsSignalled, true); }

QtImageSink::LiveBatch QtImageSink::takeLiveFrames() {
    const std::lock_guard lock(m_mutex);
    LiveBatch batch = std::exchange(m_liveNews, {});
    for (LiveChannel& open : m_liveChannels) {
        if (open.waiting) {
            batch.frames.push_back(std::move(*open.waiting));
            open.waiting.reset();
        }
    }
    m_liveNewsSignalled = false;
    return batch;
}

void QtImageSink::closeLiveChannel(const std::string& name) {
    const std::lock_guard lock(m_mutex);
    std::erase_if(m_liveChannels, [&name](const LiveChannel& open) { return open.name == name; });
}

void QtImageSink::markHandled() {
    {
        const std::lock_guard lock(m_mutex);
        if (m_pending > 0) {
            --m_pending;
        }
    }
    m_slotFreed.notify_one();
}

void QtImageSink::onRequestRejected(std::string_view reason) {
    bool first = false;
    {
        const std::lock_guard lock(m_mutex);
        first = m_rejections.empty();
        ++m_rejections[std::string(reason)];
    }
    if (first) {
        emit requestsRejected();
    }
}

std::map<std::string, int> QtImageSink::takeRejections() {
    const std::lock_guard lock(m_mutex);
    return std::exchange(m_rejections, {});
}
