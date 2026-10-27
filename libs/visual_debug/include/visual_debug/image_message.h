#pragma once

#include <opencv2/core/mat.hpp>

#include <string>

namespace visual_debug {

// One named image, as a host application sends it and a viewer receives it.
struct ImageMessage {
    std::string name;
    cv::Mat image;
};

} // namespace visual_debug
