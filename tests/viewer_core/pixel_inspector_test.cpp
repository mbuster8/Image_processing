#include "viewer_core/pixel_inspector.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <optional>
#include <string>

namespace {

using viewer_core::inspectPixel;

TEST(PixelInspector, FormatsGreyIntegerPixel) {
    cv::Mat image(1, 2, CV_8UC1, cv::Scalar(0));
    image.at<uchar>(0, 1) = 7;

    EXPECT_EQ(inspectPixel(image, 1, 0), "X=1 Y=0 · I=7");
}

TEST(PixelInspector, FormatsBgrChannelsInOpenCvOrder) {
    const cv::Mat image(1, 1, CV_8UC3, cv::Scalar(1, 2, 3));

    EXPECT_EQ(inspectPixel(image, 0, 0), "X=0 Y=0 · B=1 G=2 R=3");
}

TEST(PixelInspector, FormatsBgraChannels) {
    const cv::Mat image(1, 1, CV_8UC4, cv::Scalar(1, 2, 3, 4));

    EXPECT_EQ(inspectPixel(image, 0, 0), "X=0 Y=0 · B=1 G=2 R=3 A=4");
}

TEST(PixelInspector, FormatsTwoChannelsAsC0C1) {
    const cv::Mat image(1, 1, CV_8UC2, cv::Scalar(5, 6));

    EXPECT_EQ(inspectPixel(image, 0, 0), "X=0 Y=0 · C0=5 C1=6");
}

TEST(PixelInspector, FormatsSignedAndFloatingValues) {
    EXPECT_EQ(inspectPixel(cv::Mat(1, 1, CV_16SC1, cv::Scalar(-300)), 0, 0), "X=0 Y=0 · I=-300");
    EXPECT_EQ(inspectPixel(cv::Mat(1, 1, CV_32FC1, cv::Scalar(0.1234567)), 0, 0), "X=0 Y=0 · I=0.123457");
    EXPECT_EQ(inspectPixel(cv::Mat(1, 1, CV_64FC1, cv::Scalar(1e-9)), 0, 0), "X=0 Y=0 · I=1e-09");
}

TEST(PixelInspector, ReturnsNothingOutOfBounds) {
    const cv::Mat image(3, 4, CV_8UC1, cv::Scalar(0));

    EXPECT_EQ(inspectPixel(image, -1, 0), std::nullopt);
    EXPECT_EQ(inspectPixel(image, 0, -1), std::nullopt);
    EXPECT_EQ(inspectPixel(image, 4, 0), std::nullopt);
    EXPECT_EQ(inspectPixel(image, 0, 3), std::nullopt);
}

} // namespace
