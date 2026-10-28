#include "viewer_core/pixel_inspector.h"

#include <opencv2/core.hpp>

#include <array>
#include <charconv>
#include <string_view>

namespace viewer_core {
namespace {

std::array<std::string_view, 4> channelLabels(int channels) {
    switch (channels) {
    case 1:
        return {"I"};
    case 2:
        return {"C0", "C1"};
    case 3:
        return {"B", "G", "R"};
    default:
        return {"B", "G", "R", "A"};
    }
}

template <typename T> T valueAt(const cv::Mat& image, int x, int y, int channel) {
    return image.ptr<T>(y)[x * image.channels() + channel];
}

// std::to_chars never consults the locale, so a German system still prints a decimal point. General format
// with 6 significant digits matches printf's %g.
std::string formatFloatingPoint(double value) {
    std::array<char, 32> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general, 6);
    return std::string(buffer.data(), result.ptr);
}

std::string formatValue(const cv::Mat& image, int x, int y, int channel) {
    switch (image.depth()) {
    case CV_8U:
        return std::to_string(valueAt<uchar>(image, x, y, channel));
    case CV_8S:
        return std::to_string(valueAt<schar>(image, x, y, channel));
    case CV_16U:
        return std::to_string(valueAt<ushort>(image, x, y, channel));
    case CV_16S:
        return std::to_string(valueAt<short>(image, x, y, channel));
    case CV_32S:
        return std::to_string(valueAt<int>(image, x, y, channel));
    case CV_32F:
        return formatFloatingPoint(valueAt<float>(image, x, y, channel));
    case CV_64F:
        return formatFloatingPoint(valueAt<double>(image, x, y, channel));
    default:
        return "?";
    }
}

} // namespace

std::optional<std::string> inspectPixel(const cv::Mat& image, int x, int y) {
    if (image.empty() || image.dims != 2 || x < 0 || y < 0 || x >= image.cols || y >= image.rows ||
        image.channels() > 4) {
        return std::nullopt;
    }
    std::string text = "X=" + std::to_string(x) + " Y=" + std::to_string(y) + " ·";
    const auto labels = channelLabels(image.channels());
    for (int channel = 0; channel < image.channels(); ++channel) {
        text += ' ';
        text += labels[static_cast<std::size_t>(channel)];
        text += '=';
        text += formatValue(image, x, y, channel);
    }
    return text;
}

} // namespace viewer_core
