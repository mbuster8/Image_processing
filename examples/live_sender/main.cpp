// Streams frames to the viewer as live channels: each frame as "camera", its Canny edges as "edges". The
// program has no window of its own: the viewer is its screen, with Freeze, Snapshot and the display range.
//
//     live_sender [--camera N | <video file> | --pattern [--size WxH] [--fps N]] [--frames N]
//                 [--endpoint <uri>]
//     live_sender --list-cameras
//
// Without a source it reads camera 0. A camera runs until Ctrl+C and a file until its end, unless --frames
// stops them sooner; a summary prints either way. A file is sent at its own frame rate, the pattern at
// --fps (30 by default). Exit code: 0 when every frame was delivered, 1 when any was not, 2 for bad
// arguments or an unreadable source.

#include "live_sender.h"

#include "visual_debug/client.h"

#include <opencv2/core/utils/logger.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <variant>

namespace {

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

constexpr int kExitAllDelivered = 0;
constexpr int kExitNotDelivered = 1;
constexpr int kExitUsageError = 2;
constexpr int kCamerasToTry = 10;
constexpr auto kStopCheckInterval = 50ms; // how quickly Ctrl+C ends a wait between slow frames

constexpr const char* kUsage =
    "usage: live_sender [--camera N | <video file> | --pattern [--size WxH] [--fps N]] [--frames N]\n"
    "                   [--endpoint <uri>]\n"
    "       live_sender --list-cameras\n";

// Set by Ctrl+C. A signal handler may only touch a lock-free atomic.
std::atomic<bool> stopRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

void requestStop(int) { stopRequested = true; }

int listCameras() {
    // OpenCV logs a warning for every number that does not open; here the list is the whole output.
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);
    int found = 0;
    for (int index = 0; index < kCamerasToTry; ++index) {
        cv::VideoCapture camera(index);
        if (camera.isOpened()) {
            std::cout << "--camera " << index << ": " << camera.get(cv::CAP_PROP_FRAME_WIDTH) << 'x'
                      << camera.get(cv::CAP_PROP_FRAME_HEIGHT) << '\n';
            ++found;
        }
    }
    if (found == 0) {
        std::cout << "No camera opened (numbers 0 to " << kCamerasToTry - 1 << " tried).\n";
    }
    return kExitAllDelivered;
}

// Opens a camera or a video file. A file's own frame rate becomes the interval between frames; a camera
// delivers frames at its own pace, so it has none.
bool open(cv::VideoCapture& capture, const live_sender::Arguments& arguments,
          std::optional<Clock::duration>& interval) {
    try {
        if (const auto* camera = std::get_if<live_sender::CameraSource>(&arguments.source)) {
            return capture.open(camera->index);
        }
        const auto& file = std::get<live_sender::FileSource>(arguments.source);
        if (!capture.open(file.path)) {
            return false;
        }
        const double fps = capture.get(cv::CAP_PROP_FPS);
        if (fps >= live_sender::kMinPatternFps && fps <= live_sender::kMaxPatternFps) {
            interval = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps));
        }
        return true;
    } catch (const std::exception&) {
        return false; // reported by the caller like any other source that does not open
    }
}

void printTally(const char* channel, const live_sender::ChannelTally& tally) {
    std::cout << channel << ": " << tally.sent << " sent, " << tally.delivered << " delivered";
    for (const auto& [result, times] : tally.failed) {
        std::cout << ", " << times << ' ' << visual_debug::toString(result);
    }
    std::cout << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    const auto arguments = live_sender::parseArguments(argc, argv);
    if (!arguments) {
        std::cerr << kUsage;
        return kExitUsageError;
    }
    if (arguments->listCameras) {
        return listCameras();
    }

    cv::VideoCapture capture;
    std::optional<Clock::duration> interval;
    std::function<std::optional<cv::Mat>()> next;
    if (const auto* pattern = std::get_if<live_sender::PatternSource>(&arguments->source)) {
        interval =
            std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / pattern->fps));
        next = [size = pattern->size, index = 0]() mutable -> std::optional<cv::Mat> {
            return live_sender::patternFrame(index++, size);
        };
    } else {
        if (!open(capture, *arguments, interval)) {
            if (const auto* camera = std::get_if<live_sender::CameraSource>(&arguments->source)) {
                std::cerr << "cannot open camera " << camera->index
                          << " (live_sender --list-cameras shows the numbers that open)\n";
            } else {
                std::cerr << "cannot open " << std::get<live_sender::FileSource>(arguments->source).path
                          << '\n';
            }
            return kExitUsageError;
        }
        next = [&capture]() -> std::optional<cv::Mat> {
            cv::Mat frame;
            if (!capture.read(frame) || frame.empty()) {
                return std::nullopt;
            }
            return frame;
        };
    }

    std::optional<live_sender::FramePacer> pacer;
    if (interval) {
        pacer.emplace(*interval, Clock::now());
    }
    const auto waitAndContinue = [&pacer] {
        if (pacer) {
            while (!stopRequested && Clock::now() < pacer->due()) {
                std::this_thread::sleep_for(
                    std::min<Clock::duration>(pacer->due() - Clock::now(), kStopCheckInterval));
            }
            pacer->sent(Clock::now());
        }
        return !stopRequested;
    };

    std::signal(SIGINT, requestStop);
    visual_debug::Client client(visual_debug::ClientOptions{.endpoint = arguments->endpoint});
    const live_sender::Tally tally = live_sender::run(next, client, arguments->frames, waitAndContinue);

    if (tally.camera.sent == 0 && !stopRequested) {
        std::cerr << "no frame could be read\n";
        return kExitUsageError;
    }
    printTally("camera", tally.camera);
    printTally("edges", tally.edges);
    const bool allDelivered =
        tally.camera.delivered == tally.camera.sent && tally.edges.delivered == tally.edges.sent;
    return allDelivered ? kExitAllDelivered : kExitNotDelivered;
}
