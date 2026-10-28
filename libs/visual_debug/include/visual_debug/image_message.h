#pragma once

#include <opencv2/core/mat.hpp>

#include <chrono>
#include <string>

namespace visual_debug {

// One named image, as a host application sends it and a viewer receives it.
struct ImageMessage {
    std::string name;
    cv::Mat image;
    // Set by the Server when decoding finishes, on its own thread; not part of the wire format.
    std::chrono::system_clock::time_point receivedAt{};
};

} // namespace visual_debug
