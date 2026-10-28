#pragma once

#include <span>
#include <string_view>

namespace viewer_core {

// The file extensions, lower case and without the dot, of the formats cv::imread can open. Which of them
// actually decode depends on how OpenCV was built; a file that does not is reported, not shown.
std::span<const std::string_view> imageExtensions();

} // namespace viewer_core
