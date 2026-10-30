#include "live_sender.h"

#include "visual_debug/endpoint.h"
#include "visual_debug/wire_format.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <system_error>

namespace live_sender {
namespace {

using Source = std::variant<CameraSource, FileSource, PatternSource>;

// The whole of `text` as a whole number, or nothing.
std::optional<int> wholeNumber(std::string_view text) {
    int value = 0;
    const char* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc() || stop != end) {
        return std::nullopt;
    }
    return value;
}

// From kMinPatternFps to kMaxPatternFps: the interval between frames must fit the clock's duration type.
std::optional<double> patternFps(const std::string& text) {
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (text.empty() || end != text.c_str() + text.size() || !std::isfinite(value) ||
        value < kMinPatternFps || value > kMaxPatternFps) {
        return std::nullopt;
    }
    return value;
}

// "WxH", for a BGR picture the viewer can take.
std::optional<cv::Size> patternSize(std::string_view text) {
    const std::size_t x = text.find('x');
    if (x == std::string_view::npos) {
        return std::nullopt;
    }
    const std::optional<int> width = wholeNumber(text.substr(0, x));
    const std::optional<int> height = wholeNumber(text.substr(x + 1));
    if (!width || !height || *width < 1 || *height < 1 || *width > visual_debug::kMaxDimension ||
        *height > visual_debug::kMaxDimension ||
        static_cast<std::size_t>(*width) * static_cast<std::size_t>(*height) * 3 >
            visual_debug::kMaxPixelBytes) {
        return std::nullopt;
    }
    return cv::Size(*width, *height);
}

void count(ChannelTally& tally, visual_debug::SendResult result) {
    ++tally.sent;
    if (result == visual_debug::SendResult::Delivered) {
        ++tally.delivered;
    } else {
        ++tally.failed[result];
    }
}

} // namespace

std::optional<Arguments> parseArguments(int argc, char* argv[]) {
    Arguments arguments;
    arguments.endpoint = visual_debug::defaultEndpoint();
    std::optional<Source> source;
    std::optional<cv::Size> size;
    std::optional<double> fps;
    // Two sources are a mistake to report, not a choice to guess.
    const auto choose = [&source](Source chosen) {
        const bool first = !source;
        source = std::move(chosen);
        return first;
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        const bool hasValue = i + 1 < argc;
        if (argument == "--camera" && hasValue) {
            const std::optional<int> index = wholeNumber(argv[++i]);
            if (!index || *index < 0 || !choose(CameraSource{*index})) {
                return std::nullopt;
            }
        } else if (argument == "--pattern") {
            if (!choose(PatternSource{})) {
                return std::nullopt;
            }
        } else if (argument == "--size" && hasValue) {
            size = patternSize(argv[++i]);
            if (!size) {
                return std::nullopt;
            }
        } else if (argument == "--fps" && hasValue) {
            fps = patternFps(argv[++i]);
            if (!fps) {
                return std::nullopt;
            }
        } else if (argument == "--frames" && hasValue) {
            arguments.frames = wholeNumber(argv[++i]);
            if (!arguments.frames || *arguments.frames < 1) {
                return std::nullopt;
            }
        } else if (argument == "--endpoint" && hasValue) {
            arguments.endpoint = argv[++i];
        } else if (argument == "--list-cameras") {
            arguments.listCameras = true;
        } else if (!argument.empty() && !argument.starts_with('-')) {
            if (!choose(FileSource{std::string(argument)})) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }

    if (arguments.listCameras) {
        return argc == 2 ? std::optional(arguments) : std::nullopt;
    }
    if (size || fps) {
        PatternSource* pattern = source ? std::get_if<PatternSource>(&*source) : nullptr;
        if (!pattern) {
            return std::nullopt;
        }
        pattern->size = size.value_or(pattern->size);
        pattern->fps = fps.value_or(pattern->fps);
    }
    if (source) {
        arguments.source = std::move(*source);
    }
    return arguments;
}

cv::Mat patternFrame(int index, cv::Size size) {
    // 64-bit, so a long run cannot overflow the offsets.
    const std::int64_t frame = index;
    const std::int64_t width = size.width;
    cv::Mat row(1, size.width, CV_8UC3);
    const double span = std::max(1, size.width - 1);
    for (int x = 0; x < size.width; ++x) {
        const auto phase = static_cast<double>((x + frame * 8) % width);
        const auto level = cv::saturate_cast<uchar>(255.0 * phase / span);
        row.at<cv::Vec3b>(0, x) = cv::Vec3b(level, 96, static_cast<uchar>(255 - level));
    }
    cv::Mat picture;
    cv::repeat(row, size.height, 1, picture);

    const int discX = static_cast<int>(frame * 7 % width);
    const int shortSide = std::min(size.width, size.height);
    cv::circle(picture, {discX, size.height / 2}, shortSide / 5, cv::Scalar(255, 255, 255), cv::FILLED,
               cv::LINE_AA);
    cv::putText(picture, "frame " + std::to_string(index), {size.width / 16, size.height - size.height / 10},
                cv::FONT_HERSHEY_SIMPLEX, size.width / 640.0, cv::Scalar(0, 0, 0),
                std::max(1, size.width / 320), cv::LINE_AA);
    return picture;
}

cv::Mat edgesOf(const cv::Mat& frame) {
    cv::Mat grey = frame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, grey, cv::COLOR_BGR2GRAY);
    } else if (frame.channels() == 4) {
        cv::cvtColor(frame, grey, cv::COLOR_BGRA2GRAY);
    }
    cv::Mat edges;
    cv::Canny(grey, edges, 50, 150);
    return edges;
}

FramePacer::FramePacer(Clock::duration interval, Clock::time_point start)
    : m_interval(interval), m_due(start) {}

FramePacer::Clock::time_point FramePacer::due() const { return m_due; }

void FramePacer::sent(Clock::time_point now) {
    m_due = now - m_due < m_interval ? m_due + m_interval : now + m_interval;
}

Tally run(const std::function<std::optional<cv::Mat>()>& next, visual_debug::ImageSender& sender,
          std::optional<int> frames, const std::function<bool()>& waitAndContinue) {
    Tally tally;
    while (!frames || tally.camera.sent < *frames) {
        if (!waitAndContinue()) {
            break;
        }
        const std::optional<cv::Mat> frame = next();
        if (!frame) {
            break;
        }
        count(tally.camera, sender.sendLive(*frame, "camera"));
        count(tally.edges, sender.sendLive(edgesOf(*frame), "edges"));
    }
    return tally;
}

} // namespace live_sender
