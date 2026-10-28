#include "viewer_core/display_converter.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using viewer_core::toDisplayImage;

cv::Vec4b displayPixel(const cv::Mat& image, int row = 0, int col = 0) {
    const cv::Mat display = toDisplayImage(image);
    EXPECT_EQ(display.type(), CV_8UC4);
    return display.at<cv::Vec4b>(row, col);
}

TEST(DisplayConverter, EightBitGreyBecomesOpaqueGrey) {
    const cv::Mat image(1, 1, CV_8UC1, cv::Scalar(37));

    EXPECT_EQ(displayPixel(image), cv::Vec4b(37, 37, 37, 255));
}

TEST(DisplayConverter, EightBitBgrBecomesRgba) {
    const cv::Mat image(1, 1, CV_8UC3, cv::Scalar(10, 20, 30));

    EXPECT_EQ(displayPixel(image), cv::Vec4b(30, 20, 10, 255));
}

TEST(DisplayConverter, EightBitBgraBecomesRgba) {
    const cv::Mat image(1, 1, CV_8UC4, cv::Scalar(10, 20, 30, 40));

    EXPECT_EQ(displayPixel(image), cv::Vec4b(30, 20, 10, 40));
}

TEST(DisplayConverter, TwoChannelsMapToRedAndGreen) {
    const cv::Mat image(1, 1, CV_8UC2, cv::Scalar(5, 6));

    EXPECT_EQ(displayPixel(image), cv::Vec4b(5, 6, 0, 255));
}

TEST(DisplayConverter, NonEightBitIsNormalisedAcrossAllChannels) {
    cv::Mat image(1, 2, CV_16UC3, cv::Scalar::all(0));
    image.at<cv::Vec3w>(0, 1) = cv::Vec3w(1020, 512, 256); // B, G, R

    // One min and max for all channels (0 and 1020), so the channels keep their relative brightness.
    EXPECT_EQ(displayPixel(image, 0, 0), cv::Vec4b(0, 0, 0, 255));
    EXPECT_EQ(displayPixel(image, 0, 1), cv::Vec4b(64, 128, 255, 255));
}

TEST(DisplayConverter, SignedEightBitIsNormalised) {
    cv::Mat image(1, 2, CV_8SC1);
    image.at<schar>(0, 0) = -128;
    image.at<schar>(0, 1) = 127;

    EXPECT_EQ(displayPixel(image, 0, 0), cv::Vec4b(0, 0, 0, 255));
    EXPECT_EQ(displayPixel(image, 0, 1), cv::Vec4b(255, 255, 255, 255));
}

TEST(DisplayConverter, ConstantImageBecomesBlack) {
    const cv::Mat image(2, 2, CV_32FC1, cv::Scalar(5.0));

    const cv::Mat display = toDisplayImage(image);

    for (int row = 0; row < display.rows; ++row) {
        for (int col = 0; col < display.cols; ++col) {
            EXPECT_EQ(display.at<cv::Vec4b>(row, col), cv::Vec4b(0, 0, 0, 255)) << row << ", " << col;
        }
    }
}

TEST(DisplayConverter, NonFiniteValuesAreIgnoredAndShownBlack) {
    const float values[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity(), 0.0f, 10.0f};
    const cv::Mat image(1, 5, CV_32FC1, const_cast<float*>(values));

    const cv::Mat display = toDisplayImage(image);

    std::vector<int> red;
    for (int col = 0; col < display.cols; ++col) {
        red.push_back(display.at<cv::Vec4b>(0, col)[0]);
    }
    EXPECT_EQ(red, (std::vector<int>{0, 0, 0, 0, 255}));
}

TEST(DisplayConverter, LeavesOriginalUnmodified) {
    cv::Mat image(3, 3, CV_16UC1);
    cv::randu(image, cv::Scalar(0), cv::Scalar(60000));
    const cv::Mat before = image.clone();

    (void)toDisplayImage(image);

    EXPECT_EQ(cv::norm(image, before, cv::NORM_INF), 0.0);
}

// What the history counts for an image's display copy before converting it.
TEST(DisplayConverter, DisplayBytesAreTheConvertedImages) {
    for (const int type : {CV_8UC1, CV_16UC3, CV_32FC4, CV_64FC2}) {
        const cv::Mat image(5, 7, type, cv::Scalar::all(1));
        const cv::Mat display = toDisplayImage(image);
        EXPECT_EQ(viewer_core::displayBytes(image), display.total() * display.elemSize()) << type;
    }
}

TEST(DisplayConverter, RejectsUnsupportedInput) {
    const int threeDimensions[] = {2, 2, 2};
    const std::vector<std::pair<std::string, cv::Mat>> images = {
        {"empty", cv::Mat()},
        {"five channels", cv::Mat(2, 2, CV_8UC(5), cv::Scalar::all(0))},
        {"16-bit float", cv::Mat(2, 2, CV_16FC1, cv::Scalar(0))},
        {"three dimensions", cv::Mat(3, threeDimensions, CV_8UC1, cv::Scalar(0))},
    };

    for (const auto& [label, image] : images) {
        EXPECT_THROW((void)toDisplayImage(image), std::invalid_argument) << label;
    }
}

} // namespace
