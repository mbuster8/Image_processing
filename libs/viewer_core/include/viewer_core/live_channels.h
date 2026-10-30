#pragma once

#include <opencv2/core/mat.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace viewer_core {

// The sender's clock: whole milliseconds since the Unix epoch, the same type as visual_debug::SentTime. Not
// system_clock's own duration, which on libstdc++ is nanoseconds and ends in 2262, before the latest sentAt.
using SentTime = std::chrono::sys_time<std::chrono::milliseconds>;

inline constexpr std::chrono::seconds kIdleAfter{5};  // a channel without a frame for this long is idle
inline constexpr std::chrono::seconds kRateWindow{1}; // frame rates count the frames of the last second

// A frame the UI took from the sink: the newest of its channel, and how many older ones the sink replaced.
struct LiveFrameIn {
    std::string channel;
    cv::Mat image;
    std::chrono::system_clock::time_point receivedAt{};
    std::optional<SentTime> sentAt{};
    std::uint64_t replaced = 0;
};

struct LiveChannel {
    std::string name;
    cv::Mat shown;  // kept while frozen
    cv::Mat newest; // empty unless frozen with a newer frame aside
    std::chrono::system_clock::time_point shownReceivedAt{};
    std::optional<SentTime> shownSentAt{};
    bool idle = false;
    bool frozen = false;
    std::uint64_t received = 0; // frames the UI took
    std::uint64_t dropped = 0;  // replaced in the sink
    std::uint64_t drawn = 0;
};

struct LiveStats {
    int arrivedPerSecond = 0; // frames taken plus those the sink replaced, in the last kRateWindow
    int drawnPerSecond = 0;
    std::optional<std::chrono::milliseconds> age{}; // the shown frame's receive time minus its sentAt
};

// The viewer's live channels: per channel, the frame to show, the counters, the frame rates and whether it is
// idle or frozen. UI thread only. Times come in as arguments, so tests need no clock: steady times for rates
// and idleness, the frames' own system times for their age.
class LiveChannels {
public:
    using SteadyTime = std::chrono::steady_clock::time_point;

    struct ReceiveResult {
        bool started;      // a new channel, or one back from idle
        bool shownChanged; // false while frozen: the frame waits in `newest` instead
    };

    ReceiveResult receive(LiveFrameIn frame, SteadyTime now);
    void markDrawn(const std::string& name, SteadyTime now); // nothing for an unknown name
    std::vector<std::string> updateIdle(SteadyTime now);     // channels that have just gone idle, in order

    // Unfreezing shows `newest`, if a frame arrived while frozen. True when the shown frame changed, which is
    // only then; false for an unknown name.
    bool setFrozen(const std::string& name, bool frozen);
    bool close(const std::string& name); // false for an unknown name; a later frame starts it again

    const LiveChannel* find(const std::string& name) const;         // nullptr for an unknown name
    const std::vector<LiveChannel>& channels() const;               // in the order they started
    LiveStats stats(const std::string& name, SteadyTime now) const; // zeros and no age for an unknown name

private:
    // (when, how many frames), oldest first; entries older than kRateWindow are dropped as new ones come.
    using Events = std::deque<std::pair<SteadyTime, std::uint64_t>>;

    // What a LiveChannel does not show, at the same index in m_timings.
    struct Timing {
        Events arrivals;
        Events draws;
        SteadyTime lastArrival{};
        std::chrono::system_clock::time_point newestReceivedAt{};
        std::optional<SentTime> newestSentAt{};
    };

    std::optional<std::size_t> indexOf(const std::string& name) const;

    std::vector<LiveChannel> m_channels;
    std::vector<Timing> m_timings;
};

} // namespace viewer_core
