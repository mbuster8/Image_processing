#include "viewer_core/viewport.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <cmath>

namespace {

using viewer_core::Viewport;

constexpr double kTolerance = 1e-9;

// A 400×400 image in a 200×200 view: the fit scale is 0.5.
Viewport squareViewport() {
    Viewport viewport;
    viewport.setViewSize({200, 200});
    viewport.setImageSize({400, 400});
    return viewport;
}

const cv::Point2d kViewCentre{100, 100};

void expectNear(cv::Point2d actual, cv::Point2d expected) {
    EXPECT_NEAR(actual.x, expected.x, kTolerance);
    EXPECT_NEAR(actual.y, expected.y, kTolerance);
}

bool isFinite(cv::Point2d point) { return std::isfinite(point.x) && std::isfinite(point.y); }

bool isFinite(cv::Rect2d rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.width) &&
           std::isfinite(rect.height);
}

TEST(Viewport, FitsAndCentresImage) {
    Viewport viewport;
    viewport.setViewSize({200, 100});
    viewport.setImageSize({400, 100});

    EXPECT_NEAR(viewport.scale(), 0.5, kTolerance);
    expectNear(viewport.imageToView({0, 0}), {0, 25});
    expectNear(viewport.imageToView({400, 100}), {200, 75});
}

TEST(Viewport, ZoomKeepsPointUnderCursorFixed) {
    auto viewport = squareViewport();
    const cv::Point2d cursor{60, 80};
    const auto before = viewport.viewToImage(cursor);

    viewport.zoomAt(cursor, 2);

    expectNear(viewport.viewToImage(cursor), before);
}

TEST(Viewport, DoesNotZoomOutBeyondFit) {
    auto viewport = squareViewport();

    viewport.zoomAt(kViewCentre, -3);

    EXPECT_NEAR(viewport.zoom(), 1.0, kTolerance);
}

TEST(Viewport, FractionalStepsScaleProportionally) {
    auto viewport = squareViewport();

    viewport.zoomAt(kViewCentre, 0.5); // half a wheel notch, as trackpads send

    EXPECT_NEAR(viewport.zoom(), std::sqrt(1.25), kTolerance);
}

TEST(Viewport, LimitsZoomToThreeVisiblePixels) {
    auto viewport = squareViewport();

    viewport.zoomAt(kViewCentre, 1000);

    const auto visible = viewport.visibleImageRect();
    EXPECT_GE(visible.width, 3.0 - kTolerance);
    EXPECT_GE(visible.height, 3.0 - kTolerance);
    EXPECT_NEAR(std::min(visible.width, visible.height), 3.0, kTolerance);
}

TEST(Viewport, PanIsClampedToImageBounds) {
    auto viewport = squareViewport();
    viewport.zoomAt(kViewCentre, 4);

    viewport.panBy({10000, 10000});

    const auto visible = viewport.visibleImageRect();
    EXPECT_GE(visible.x, -kTolerance);
    EXPECT_GE(visible.y, -kTolerance);
    EXPECT_LE(visible.x + visible.width, 400 + kTolerance);
    EXPECT_LE(visible.y + visible.height, 400 + kTolerance);
    // Clamping keeps the view full of image, not just inside it.
    EXPECT_NEAR(visible.width, 200 / viewport.scale(), kTolerance);
}

TEST(Viewport, PanDoesNothingAtFit) {
    auto viewport = squareViewport();
    const auto before = viewport.imageToView({0, 0});

    viewport.panBy({50, 50});

    expectNear(viewport.imageToView({0, 0}), before);
}

TEST(Viewport, ResizeKeepsCentrePoint) {
    auto viewport = squareViewport();
    viewport.zoomAt(kViewCentre, 3);
    const auto centre = viewport.viewToImage(kViewCentre);

    viewport.setViewSize({300, 300});

    expectNear(viewport.viewToImage({150, 150}), centre);
}

TEST(Viewport, ViewToImageRoundTrips) {
    auto viewport = squareViewport();
    viewport.zoomAt({30, 170}, 5);
    viewport.panBy({-40, 25});
    const cv::Point2d point{12.5, 187.25};

    expectNear(viewport.imageToView(viewport.viewToImage(point)), point);
}

TEST(Viewport, HandlesZeroViewSize) {
    auto viewport = squareViewport();

    viewport.setViewSize({0, 0}); // a minimised window
    viewport.zoomAt({0, 0}, 1);
    viewport.panBy({10, 10});

    EXPECT_TRUE(std::isfinite(viewport.scale()));
    EXPECT_TRUE(std::isfinite(viewport.zoom()));
    EXPECT_TRUE(isFinite(viewport.viewToImage({5, 5})));
    EXPECT_TRUE(isFinite(viewport.imageToView({5, 5})));
    EXPECT_TRUE(isFinite(viewport.visibleImageRect()));
}

TEST(Viewport, HandlesOnePixelImage) {
    Viewport viewport;
    viewport.setViewSize({200, 200});
    viewport.setImageSize({1, 1});

    viewport.zoomAt(kViewCentre, 5);

    EXPECT_NEAR(viewport.zoom(), 1.0, kTolerance);
    EXPECT_TRUE(std::isfinite(viewport.scale()));
    EXPECT_TRUE(isFinite(viewport.viewToImage({5, 5})));
    EXPECT_TRUE(isFinite(viewport.visibleImageRect()));
}

TEST(Viewport, HandlesNoImage) {
    Viewport viewport;
    viewport.setViewSize({200, 200});

    viewport.zoomAt(kViewCentre, 1);

    EXPECT_TRUE(std::isfinite(viewport.scale()));
    EXPECT_TRUE(isFinite(viewport.viewToImage({5, 5})));
    EXPECT_TRUE(isFinite(viewport.visibleImageRect()));
}

} // namespace
