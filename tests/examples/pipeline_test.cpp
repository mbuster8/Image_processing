#include "pipeline.h"

#include "visual_debug/wire_format.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <string>
#include <utility>
#include <vector>

namespace {

TEST(Pipeline, TestPatternIsColourAndSized) {
    const cv::Mat pattern = pipeline_demo::makeTestPattern();

    EXPECT_EQ(pattern.type(), CV_8UC3);
    EXPECT_EQ(pattern.size(), cv::Size(640, 480));
}

TEST(Pipeline, StagesProduceTheDocumentedTypes) {
    const std::vector<std::pair<std::string, int>> expected{
        {"Original", CV_8UC3},           {"With alpha", CV_8UC4},     {"Greyscale", CV_8UC1},
        {"Gaussian blur", CV_8UC1},      {"Sobel X", CV_16SC1},       {"Canny edges", CV_8UC1},
        {"Distance transform", CV_32FC1}, {"Laplacian", CV_64FC1},     {"Centre crop", CV_8UC3},
        {"Multi-channel 16-bit", CV_16UC3}};
    const cv::Mat original = pipeline_demo::makeTestPattern();

    const auto stages = pipeline_demo::buildPipeline();

    ASSERT_EQ(stages.size(), expected.size());
    for (std::size_t i = 0; i < stages.size(); ++i) {
        SCOPED_TRACE(expected[i].first);
        EXPECT_EQ(stages[i].name, expected[i].first);
        const cv::Mat output = stages[i].run(original);
        EXPECT_EQ(output.type(), expected[i].second);
        EXPECT_TRUE(visual_debug::isSupportedImage(output)); // every stage can be sent
    }
}

TEST(Pipeline, CentreCropIsANonContinuousRegionOfInterest) {
    const cv::Mat original = pipeline_demo::makeTestPattern();

    const cv::Mat crop = pipeline_demo::buildPipeline().at(8).run(original);

    EXPECT_FALSE(crop.isContinuous());
    EXPECT_EQ(crop.size(), cv::Size(320, 240));
}

TEST(Pipeline, StagesLeaveTheOriginalUnchanged) {
    const cv::Mat original = pipeline_demo::makeTestPattern();
    const cv::Mat copy = original.clone();

    for (const auto& stage : pipeline_demo::buildPipeline()) {
        (void)stage.run(original);
    }

    EXPECT_EQ(cv::norm(original, copy, cv::NORM_INF), 0.0);
}

TEST(Pipeline, WorksOnGreyscaleInput) {
    const cv::Mat grey(48, 64, CV_8UC1, cv::Scalar(100));

    for (const auto& stage : pipeline_demo::buildPipeline()) {
        SCOPED_TRACE(stage.name);
        EXPECT_TRUE(visual_debug::isSupportedImage(stage.run(grey)));
    }
}

} // namespace
