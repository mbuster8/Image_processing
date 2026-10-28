#pragma once

#include <opencv2/core/mat.hpp>

#include <optional>
#include <string>

namespace viewer_core {

// The original value of every channel at image coordinate (x, y), for example "X=12 Y=40 · B=12 G=200 R=3",
// or nothing outside the image. Labels follow OpenCV's channel order: I; C0 C1; B G R; B G R A.
std::optional<std::string> inspectPixel(const cv::Mat& image, int x, int y);

} // namespace viewer_core
