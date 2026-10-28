#pragma once

#include <opencv2/core/mat.hpp>

#include <cstddef>

namespace viewer_core {

// Converts a supported image (2-D, depth 8U to 64F, 1-4 channels) to 8-bit RGBA (CV_8UC4) for display.
// 8U values are shown as they are; other depths are min-max normalised to 0-255 across all channels together,
// ignoring NaN and infinities, which are shown as 0. Throws std::invalid_argument for anything else.
cv::Mat toDisplayImage(const cv::Mat& image);

// The bytes toDisplayImage's result takes for this image, known without converting it.
std::size_t displayBytes(const cv::Mat& image);

} // namespace viewer_core
