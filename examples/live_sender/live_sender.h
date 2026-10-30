#pragma once

#include "visual_debug/image_sender.h"

#include <opencv2/core/mat.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace live_sender {

struct CameraSource {
    int index = 0;
};

struct FileSource {
    std::string path;
};

// A generated moving picture, sent at `fps` frames a second.
struct PatternSource {
    cv::Size size{640, 480};
    double fps = 30.0;
};

struct Arguments {
    std::variant<CameraSource, FileSource, PatternSource> source = CameraSource{};
    std::optional<int> frames; // stop after this many; otherwise at the source's end or on Ctrl+C
    std::string endpoint;      // visual_debug::defaultEndpoint() unless given
    bool listCameras = false;
};

inline constexpr double kMinPatternFps = 0.1;
inline constexpr double kMaxPatternFps = 1000.0;

// live_sender [--camera N | <video file> | --pattern [--size WxH] [--fps N]] [--frames N] [--endpoint <uri>]
// live_sender --list-cameras
// Nothing for anything else: an unknown option, a missing or malformed value, two sources, --size or --fps
// without --pattern, a pattern the viewer could not take or a rate outside kMinPatternFps to
// kMaxPatternFps, or --list-cameras with other arguments.
std::optional<Arguments> parseArguments(int argc, char* argv[]);

// A BGR test picture that moves (a scrolling gradient, a disc crossing it, the frame number), so consecutive
// indices give different frames and the edges channel has outlines to show. `index` from 0, size from 1×1.
cv::Mat patternFrame(int index, cv::Size size);

// The Canny edges (thresholds 50 and 150) of an 8-bit greyscale, BGR or BGRA frame: CV_8UC1, the same size.
cv::Mat edgesOf(const cv::Mat& frame);

struct ChannelTally {
    int sent = 0;
    int delivered = 0;
    std::map<visual_debug::SendResult, int> failed; // how often each other result came back
};

struct Tally {
    ChannelTally camera;
    ChannelTally edges;
};

// Until `frames` frames have been sent, `next` has none left, or `waitAndContinue` returns false: calls
// `waitAndContinue` (which paces the frames and watches for Ctrl+C), reads a frame, and sends it as live
// channel "camera" and its edges as live channel "edges".
Tally run(const std::function<std::optional<cv::Mat>()>& next, visual_debug::ImageSender& sender,
          std::optional<int> frames, const std::function<bool()>& waitAndContinue);

} // namespace live_sender
