#include "viewer_core/live_channels.h"

#include <algorithm>
#include <limits>

namespace viewer_core {
namespace {

using Events = std::deque<std::pair<LiveChannels::SteadyTime, std::uint64_t>>;

void record(Events& events, LiveChannels::SteadyTime now, std::uint64_t frames) {
    events.emplace_back(now, frames);
    while (now - events.front().first >= kRateWindow) {
        events.pop_front();
    }
}

int framesWithinWindow(const Events& events, LiveChannels::SteadyTime now) {
    std::uint64_t frames = 0;
    for (const auto& [when, count] : events) {
        if (now - when < kRateWindow) {
            frames += count;
        }
    }
    return static_cast<int>(std::min<std::uint64_t>(frames, std::numeric_limits<int>::max()));
}

} // namespace

LiveChannels::ReceiveResult LiveChannels::receive(LiveFrameIn frame, SteadyTime now) {
    ReceiveResult result{.started = false, .shownChanged = false};
    std::optional<std::size_t> index = indexOf(frame.channel);
    if (!index) {
        LiveChannel channel;
        channel.name = std::move(frame.channel);
        m_channels.push_back(std::move(channel));
        m_timings.emplace_back();
        index = m_channels.size() - 1;
        result.started = true;
    }
    LiveChannel& channel = m_channels[*index];
    Timing& timing = m_timings[*index];
    if (channel.idle) {
        channel.idle = false;
        result.started = true;
    }
    channel.received += 1;
    channel.dropped += frame.replaced;
    timing.lastArrival = now;
    record(timing.arrivals, now, 1 + frame.replaced);
    if (channel.frozen) {
        channel.newest = std::move(frame.image);
        timing.newestReceivedAt = frame.receivedAt;
        timing.newestSentAt = frame.sentAt;
    } else {
        channel.shown = std::move(frame.image);
        channel.shownReceivedAt = frame.receivedAt;
        channel.shownSentAt = frame.sentAt;
        result.shownChanged = true;
    }
    return result;
}

void LiveChannels::markDrawn(const std::string& name, SteadyTime now) {
    if (const std::optional<std::size_t> index = indexOf(name)) {
        m_channels[*index].drawn += 1;
        record(m_timings[*index].draws, now, 1);
    }
}

std::vector<std::string> LiveChannels::updateIdle(SteadyTime now) {
    std::vector<std::string> wentIdle;
    for (std::size_t index = 0; index < m_channels.size(); ++index) {
        LiveChannel& channel = m_channels[index];
        if (!channel.idle && now - m_timings[index].lastArrival >= kIdleAfter) {
            channel.idle = true;
            wentIdle.push_back(channel.name);
        }
    }
    return wentIdle;
}

bool LiveChannels::setFrozen(const std::string& name, bool frozen) {
    const std::optional<std::size_t> index = indexOf(name);
    if (!index) {
        return false;
    }
    LiveChannel& channel = m_channels[*index];
    channel.frozen = frozen;
    if (frozen || channel.newest.empty()) {
        return false;
    }
    const Timing& timing = m_timings[*index];
    channel.shown = std::move(channel.newest);
    channel.newest = cv::Mat();
    channel.shownReceivedAt = timing.newestReceivedAt;
    channel.shownSentAt = timing.newestSentAt;
    return true;
}

bool LiveChannels::close(const std::string& name) {
    const std::optional<std::size_t> index = indexOf(name);
    if (!index) {
        return false;
    }
    const auto offset = static_cast<std::ptrdiff_t>(*index);
    m_channels.erase(m_channels.begin() + offset);
    m_timings.erase(m_timings.begin() + offset);
    return true;
}

const LiveChannel* LiveChannels::find(const std::string& name) const {
    const std::optional<std::size_t> index = indexOf(name);
    return index ? &m_channels[*index] : nullptr;
}

const std::vector<LiveChannel>& LiveChannels::channels() const { return m_channels; }

LiveStats LiveChannels::stats(const std::string& name, SteadyTime now) const {
    const std::optional<std::size_t> index = indexOf(name);
    if (!index) {
        return {};
    }
    const LiveChannel& channel = m_channels[*index];
    const Timing& timing = m_timings[*index];
    LiveStats stats{.arrivedPerSecond = framesWithinWindow(timing.arrivals, now),
                    .drawnPerSecond = framesWithinWindow(timing.draws, now)};
    // In milliseconds throughout: converting the latest sentAt to system_clock's nanoseconds would overflow.
    if (channel.shownSentAt) {
        stats.age =
            std::chrono::floor<std::chrono::milliseconds>(channel.shownReceivedAt) - *channel.shownSentAt;
    }
    return stats;
}

std::optional<std::size_t> LiveChannels::indexOf(const std::string& name) const {
    const auto found = std::find_if(m_channels.begin(), m_channels.end(),
                                    [&name](const LiveChannel& channel) { return channel.name == name; });
    if (found == m_channels.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - m_channels.begin());
}

} // namespace viewer_core
