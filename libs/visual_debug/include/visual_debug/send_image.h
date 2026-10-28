#pragma once

#include "visual_debug/image_sender.h"

#include <opencv2/core/mat.hpp>

#include <string_view>

namespace visual_debug {

// The one-line debugging call: sends an image to the viewer at defaultEndpoint() through one process-wide
// Client. Safe from any thread (calls are serialised). Never throws.
SendResult sendImage(const cv::Mat& image, std::string_view name) noexcept;

} // namespace visual_debug
