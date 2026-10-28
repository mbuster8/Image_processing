// Runs an image through a list of OpenCV stages and sends each result to the viewer, the way an ordinary
// OpenCV program would use visual_debug while it is being developed.
//
//     pipeline_demo [image-path] [--endpoint <uri>]
//
// Without a path it uses a generated test pattern. Exit code: 0 when every stage was delivered, 1 when any
// was not, 2 for bad arguments or an unreadable image.

#include "pipeline.h"

#include "visual_debug/client.h"
#include "visual_debug/endpoint.h"
#include "visual_debug/wire_format.h"

#include <opencv2/imgcodecs.hpp>

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr int kExitAllDelivered = 0;
constexpr int kExitNotDelivered = 1;
constexpr int kExitUsageError = 2;

struct Arguments {
    std::optional<std::string> imagePath;
    std::string endpoint;
};

std::optional<Arguments> parseArguments(int argc, char* argv[]) {
    Arguments arguments{.imagePath = std::nullopt, .endpoint = visual_debug::defaultEndpoint()};
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--endpoint" && i + 1 < argc) {
            arguments.endpoint = argv[++i];
        } else if (!argument.starts_with('-') && !arguments.imagePath) {
            arguments.imagePath = std::string(argument);
        } else {
            return std::nullopt;
        }
    }
    return arguments;
}

// Read as 8-bit colour: the stages make the 16-bit and floating-point images themselves.
std::optional<cv::Mat> readImage(const std::string& path) {
    try {
        cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
        if (visual_debug::isSupportedImage(image)) {
            return image;
        }
    } catch (const std::exception&) {
        // reported below like any other unreadable file
    }
    return std::nullopt;
}

} // namespace

int main(int argc, char* argv[]) {
    const auto arguments = parseArguments(argc, argv);
    if (!arguments) {
        std::cerr << "usage: pipeline_demo [image-path] [--endpoint <uri>]\n";
        return kExitUsageError;
    }

    cv::Mat original = pipeline_demo::makeTestPattern();
    if (arguments->imagePath) {
        const auto image = readImage(*arguments->imagePath);
        if (!image) {
            std::cerr << "cannot read an image from " << *arguments->imagePath << '\n';
            return kExitUsageError;
        }
        original = *image;
    }

    visual_debug::Client client(visual_debug::ClientOptions{.endpoint = arguments->endpoint});
    bool allDelivered = true;
    for (const auto& stage : pipeline_demo::buildPipeline()) {
        const visual_debug::SendResult result = client.send(stage.run(original), stage.name);
        std::cout << stage.name << ": " << visual_debug::toString(result) << '\n';
        allDelivered = allDelivered && result == visual_debug::SendResult::Delivered;
    }
    return allDelivered ? kExitAllDelivered : kExitNotDelivered;
}
