#include "viewer_core/display_converter.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

namespace viewer_core {
namespace {

bool isFloatingPoint(int depth) { return depth == CV_32F || depth == CV_64F; }

void checkSupported(const cv::Mat& image) {
    if (image.empty() || image.dims != 2 || image.depth() > CV_64F || image.channels() > 4) {
        throw std::invalid_argument(
            "toDisplayImage: expected a 2-D image of depth 8U to 64F with 1-4 channels");
    }
}

// 255 where the value is finite. NaN fails every comparison, so one "|v| <= max" test catches NaN and ±Inf.
cv::Mat finiteMask(const cv::Mat& values) {
    const double largest =
        values.depth() == CV_32F ? std::numeric_limits<float>::max() : std::numeric_limits<double>::max();
    const cv::Mat magnitudes = cv::abs(values);
    cv::Mat mask;
    cv::compare(magnitudes, largest, mask, cv::CMP_LE);
    return mask;
}

// An image's channels side by side in one plane, so one range covers them all and they keep their relative
// brightness. For floating point, NaN and infinities are set to 0, away from the arithmetic, and `finite`
// marks the others; integer depths have neither, and leave `finite` empty.
struct Plane {
    cv::Mat values;
    cv::Mat finite;
};

Plane planeOf(const cv::Mat& image) {
    Plane plane{image.reshape(1), {}};
    if (isFloatingPoint(image.depth())) {
        plane.finite = finiteMask(plane.values);
        plane.values = plane.values.clone();
        plane.values.setTo(0, ~plane.finite);
    }
    return plane;
}

DisplayRange finiteRange(const Plane& plane) {
    DisplayRange range;
    cv::minMaxIdx(plane.values, &range.min, &range.max, nullptr, nullptr, plane.finite);
    return range;
}

// range.min becomes 0 and range.max 255, with values outside clipped. An empty range is black, and so are
// non-finite values.
cv::Mat toEightBit(const Plane& plane, const DisplayRange& range, int channels) {
    cv::Mat result;
    const double span = range.max - range.min;
    if (span > 0.0) {
        plane.values.convertTo(result, CV_8U, 255.0 / span, -range.min * 255.0 / span);
    } else {
        result = cv::Mat::zeros(plane.values.size(), CV_8U);
    }
    if (!plane.finite.empty()) {
        result.setTo(0, ~plane.finite);
    }
    return result.reshape(channels);
}

cv::Mat toRgba(const cv::Mat& eightBit) {
    cv::Mat display;
    switch (eightBit.channels()) {
    case 1:
        cv::cvtColor(eightBit, display, cv::COLOR_GRAY2RGBA);
        break;
    case 2: {
        // Two channels are typically a vector field such as optical flow: show them as red and green.
        std::vector<cv::Mat> planes;
        cv::split(eightBit, planes);
        planes.push_back(cv::Mat::zeros(eightBit.size(), CV_8UC1));
        planes.push_back(cv::Mat(eightBit.size(), CV_8UC1, cv::Scalar(255)));
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

cv::Mat colourMapped(const cv::Mat& eightBit, ColourMap colours, const cv::Mat& finite) {
    cv::Mat bgr;
    cv::applyColorMap(eightBit, bgr, colours == ColourMap::Turbo ? cv::COLORMAP_TURBO : cv::COLORMAP_VIRIDIS);
    cv::Mat display;
    cv::cvtColor(bgr, display, cv::COLOR_BGR2RGBA);
    // Every map gives 0 a colour of its own, so non-finite values would no longer stand out as black.
    if (!finite.empty()) {
        display.setTo(cv::Scalar(0, 0, 0, 255), ~finite);
    }
    return display;
}

} // namespace

cv::Mat toDisplayImage(const cv::Mat& image) { return toDisplayImage(image, DisplaySettings{}); }

cv::Mat toDisplayImage(const cv::Mat& image, const DisplaySettings& settings) {
    checkSupported(image);
    cv::Mat eightBit;
    cv::Mat finite;
    if (settings.mode == RangeMode::Auto && image.depth() == CV_8U) {
        eightBit = image;
    } else {
        const Plane plane = planeOf(image);
        const DisplayRange range = settings.mode == RangeMode::Auto ? finiteRange(plane) : settings.range;
        eightBit = toEightBit(plane, range, image.channels());
        finite = plane.finite;
    }
    if (image.channels() == 1 && settings.colours != ColourMap::Grey) {
        return colourMapped(eightBit, settings.colours, finite);
    }
    return toRgba(eightBit);
}

DisplayRange autoRange(const cv::Mat& image) {
    checkSupported(image);
    if (image.depth() == CV_8U) {
        return {0.0, 255.0};
    }
    return finiteRange(planeOf(image));
}

std::size_t displayBytes(const cv::Mat& image) { return image.total() * 4; } // 8-bit RGBA

} // namespace viewer_core
