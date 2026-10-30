#include "viewer_core/display_converter.h"
#include "viewer_core/display_settings.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <limits>
#include <vector>

namespace {

using viewer_core::ColourMap;
using viewer_core::DisplayRange;
using viewer_core::DisplaySettings;
using viewer_core::RangeMode;

// The red channel of a display pixel: for a grey picture, its grey level.
int greyAt(const cv::Mat& display, int x) { return display.at<cv::Vec4b>(0, x)[0]; }

cv::Mat row(int type, const std::vector<double>& values) {
    cv::Mat image(1, static_cast<int>(values.size()), type);
    for (int x = 0; x < image.cols; ++x) {
        image.col(x).setTo(values[static_cast<std::size_t>(x)]);
    }
    return image;
}

double maxDifference(const cv::Mat& a, const cv::Mat& b) { return cv::norm(a, b, cv::NORM_INF); }

TEST(DisplaySettings, AutoGreyEqualsTodaysOutput) {
    constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
    std::vector<cv::Mat> images{row(CV_8UC3, {1, 2, 3}), row(CV_16UC1, {100, 900, 400}),
                                row(CV_32FC1, {kNan, -2.5, 7.0}), row(CV_64FC2, {0.25, -1.0})};

    for (const cv::Mat& image : images) {
        EXPECT_EQ(maxDifference(viewer_core::toDisplayImage(image, DisplaySettings{}),
                                viewer_core::toDisplayImage(image)),
                  0.0)
            << image.type();
    }
}

TEST(DisplaySettings, AutoRangeOfEightBitIsFull) {
    constexpr float kInf = std::numeric_limits<float>::infinity();
    constexpr float kNan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_EQ(viewer_core::autoRange(row(CV_8UC1, {7, 7})), (DisplayRange{0, 255}));
    EXPECT_EQ(viewer_core::autoRange(row(CV_16UC1, {100, 900, 400})), (DisplayRange{100, 900}));
    EXPECT_EQ(viewer_core::autoRange(row(CV_32FC1, {kNan, 1, kInf, 3, -kInf})), (DisplayRange{1, 3}));
}

TEST(DisplaySettings, FixedRangeClips) {
    const cv::Mat image = row(CV_16UC1, {0, 100, 200, 300});
    const DisplaySettings fixed{.mode = RangeMode::Fixed, .range = {100, 200}};

    const cv::Mat display = viewer_core::toDisplayImage(image, fixed);

    EXPECT_EQ(greyAt(display, 0), 0);
    EXPECT_EQ(greyAt(display, 1), 0);
    EXPECT_EQ(greyAt(display, 2), 255);
    EXPECT_EQ(greyAt(display, 3), 255);
}

TEST(DisplaySettings, HoldAndFixedApplyToEveryDepth) {
    for (const int depth : {CV_8U, CV_16S, CV_32F, CV_64F}) {
        for (const RangeMode mode : {RangeMode::Hold, RangeMode::Fixed}) {
            const cv::Mat image = row(CV_MAKETYPE(depth, 1), {10, 20});
            const cv::Mat display = viewer_core::toDisplayImage(image, {.mode = mode, .range = {10, 20}});
            EXPECT_EQ(greyAt(display, 0), 0) << depth;
            EXPECT_EQ(greyAt(display, 1), 255) << depth;
        }
    }
}

TEST(DisplaySettings, EmptyRangeShowsBlack) {
    const cv::Mat display =
        viewer_core::toDisplayImage(row(CV_16UC1, {4, 5, 6}), {.mode = RangeMode::Fixed, .range = {5, 5}});

    for (int x = 0; x < display.cols; ++x) {
        EXPECT_EQ(display.at<cv::Vec4b>(0, x), cv::Vec4b(0, 0, 0, 255)) << x;
    }
}

TEST(DisplaySettings, ColourMapChangesSingleChannelOnly) {
    cv::Mat ramp(1, 256, CV_8UC1);
    for (int x = 0; x < ramp.cols; ++x) {
        ramp.at<unsigned char>(0, x) = static_cast<unsigned char>(x);
    }
    cv::Mat expectedBgr;
    cv::applyColorMap(ramp, expectedBgr, cv::COLORMAP_TURBO);
    cv::Mat expected;
    cv::cvtColor(expectedBgr, expected, cv::COLOR_BGR2RGBA);

    const cv::Mat turbo = viewer_core::toDisplayImage(ramp, {.colours = ColourMap::Turbo});

    EXPECT_EQ(maxDifference(turbo, expected), 0.0);
    EXPECT_GT(maxDifference(turbo, viewer_core::toDisplayImage(ramp)), 0.0);
    const cv::Mat colour = row(CV_8UC3, {10, 200});
    EXPECT_EQ(maxDifference(viewer_core::toDisplayImage(colour, {.colours = ColourMap::Turbo}),
                            viewer_core::toDisplayImage(colour)),
              0.0);
}

TEST(DisplaySettings, NanStaysBlackUnderEveryMap) {
    const cv::Mat image = row(CV_32FC1, {std::numeric_limits<double>::quiet_NaN(), 0, 1});

    for (const ColourMap colours : {ColourMap::Grey, ColourMap::Turbo, ColourMap::Viridis}) {
        const cv::Mat display = viewer_core::toDisplayImage(image, {.colours = colours});
        EXPECT_EQ(display.at<cv::Vec4b>(0, 0), cv::Vec4b(0, 0, 0, 255)) << static_cast<int>(colours);
    }
}

TEST(DisplaySettings, StoreRemembersPerName) {
    viewer_core::DisplaySettingsStore store;
    const DisplaySettings turbo{.colours = ColourMap::Turbo};
    const DisplaySettings fixed{.mode = RangeMode::Fixed, .range = {1, 2}};

    EXPECT_EQ(store.get("a"), DisplaySettings{});
    store.set("a", turbo);
    store.set("b", fixed);

    EXPECT_EQ(store.get("a"), turbo);
    EXPECT_EQ(store.get("b"), fixed);
    store.set("a", DisplaySettings{});
    EXPECT_EQ(store.get("a"), DisplaySettings{});
}

} // namespace
