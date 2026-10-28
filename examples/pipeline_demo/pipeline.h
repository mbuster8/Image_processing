#pragma once

#include <opencv2/core/mat.hpp>

#include <functional>
#include <string>
#include <vector>

namespace pipeline_demo {

// One step of an image-processing pipeline. Every stage takes the original image, so stages stay independent:
// a stage can be added, removed or reordered without touching the others.
struct PipelineStage {
    std::string name;
    std::function<cv::Mat(const cv::Mat& original)> run;
};

// Ten stages chosen to exercise every display path of the viewer: 8-bit colour and alpha, greyscale, signed
// 16-bit, 32- and 64-bit floating point, a non-continuous region of interest and 16-bit colour. Accepts
// greyscale, BGR or BGRA input.
std::vector<PipelineStage> buildPipeline();

// A BGR test image with a gradient, shapes and text, so every stage has something to show.
cv::Mat makeTestPattern(cv::Size size = {640, 480});

} // namespace pipeline_demo
