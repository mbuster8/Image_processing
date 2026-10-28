#include "viewer_core/display_converter.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

namespace viewer_core {
namespace {

bool isFloatingPoint(int depth) { return depth == CV_32F || depth == CV_64F; }

// 255 where the value is finite. NaN fails every comparison, so one "|v| <= max" test catches NaN and ±Inf.
cv::Mat finiteMask(const cv::Mat& values) {
    const double largest =
        values.depth() == CV_32F ? std::numeric_limits<float>::max() : std::numeric_limits<double>::max();
    const cv::Mat magnitudes = cv::abs(values);
    cv::Mat mask;
    cv::compare(magnitudes, largest, mask, cv::CMP_LE);
    return mask;
}

// One minimum and maximum across all channels, so the channels keep their relative brightness. A constant
// image becomes black, and non-finite values are left out of the range and shown as 0.
cv::Mat normaliseToEightBit(const cv::Mat& image) {
    cv::Mat values = image.reshape(1); // channels side by side, one plane
    cv::Mat mask;
    if (isFloatingPoint(image.depth())) {
        mask = finiteMask(values);
        values = values.clone();
        values.setTo(0, ~mask); // keep NaN and infinities away from the arithmetic below
    }

    double minimum = 0.0;
    double maximum = 0.0;
    cv::minMaxIdx(values, &minimum, &maximum, nullptr, nullptr, mask);

    cv::Mat result;
    const double range = maximum - minimum;
    if (range > 0.0) {
        values.convertTo(result, CV_8U, 255.0 / range, -minimum * 255.0 / range);
    } else {
        result = cv::Mat::zeros(values.size(), CV_8U);
    }
    if (!mask.empty()) {
        result.setTo(0, ~mask);
    }
    return result.reshape(image.channels());
}

} // namespace

cv::Mat toDisplayImage(const cv::Mat& image) {
    if (image.empty() || image.dims != 2 || image.depth() > CV_64F || image.channels() > 4) {
        throw std::invalid_argument(
            "toDisplayImage: expected a 2-D image of depth 8U to 64F with 1-4 channels");
    }

    const cv::Mat eightBit = image.depth() == CV_8U ? image : normaliseToEightBit(image);
    cv::Mat display;
    switch (image.channels()) {
    case 1:
        cv::cvtColor(eightBit, display, cv::COLOR_GRAY2RGBA);
        break;
    case 2: {
        // Two channels are typically a vector field such as optical flow: show them as red and green.
        std::vector<cv::Mat> planes;
        cv::split(eightBit, planes);
        planes.push_back(cv::Mat::zeros(image.size(), CV_8UC1));
        planes.push_back(cv::Mat(image.size(), CV_8UC1, cv::Scalar(255)));
        cv::merge(planes, display);
        break;
    }
    case 3:
        cv::cvtColor(eightBit, display, cv::COLOR_BGR2RGBA);
        break;
    default:
        cv::cvtColor(eightBit, display, cv::COLOR_BGRA2RGBA);
        break;
    }
    return display;
}

std::size_t displayBytes(const cv::Mat& image) { return image.total() * 4; } // 8-bit RGBA

} // namespace viewer_core
