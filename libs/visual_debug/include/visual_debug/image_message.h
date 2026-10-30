#pragma once

#include <opencv2/core/mat.hpp>

#include <chrono>
#include <optional>
#include <string>

namespace visual_debug {

// A sender's clock in whole milliseconds since the Unix epoch, as the header's sentAt carries it. Not the
// system clock's own duration: libstdc++ counts nanoseconds, which end in 2262, before sentAt's range does.
using SentTime = std::chrono::sys_time<std::chrono::milliseconds>;

// One named image, as a host application sends it and a viewer receives it.
struct ImageMessage {
    std::string name;
    cv::Mat image;
    // Set by the Server when decoding finishes, on its own thread; not part of the wire format.
    std::chrono::system_clock::time_point receivedAt{};
    // A frame of a live channel (the name is the channel's), not an image for the history.
    bool live = false;
    std::optional<SentTime> sentAt{};
};

} // namespace visual_debug
