#include "qimage_conversion.h"

#include <QColor>
#include <QImage>
#include <QSize>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <stdexcept>

namespace {

TEST(QImageConversion, CopiesRgbaPixels) {
    cv::Mat rgba(1, 2, CV_8UC4);
    rgba.at<cv::Vec4b>(0, 0) = cv::Vec4b(10, 20, 30, 40);
    rgba.at<cv::Vec4b>(0, 1) = cv::Vec4b(50, 60, 70, 80);

    const QImage image = qt_common::toQImage(rgba);
    rgba.setTo(cv::Scalar::all(0)); // the QImage must not share the Mat's pixels

    EXPECT_EQ(image.format(), QImage::Format_RGBA8888);
    EXPECT_EQ(image.size(), QSize(2, 1));
    EXPECT_EQ(image.pixelColor(0, 0), QColor(10, 20, 30, 40));
    EXPECT_EQ(image.pixelColor(1, 0), QColor(50, 60, 70, 80));
}

TEST(QImageConversion, RejectsOtherTypes) {
    EXPECT_THROW((void)qt_common::toQImage(cv::Mat(1, 1, CV_8UC3, cv::Scalar::all(0))),
                 std::invalid_argument);
    EXPECT_THROW((void)qt_common::wrapAsQImage(cv::Mat(1, 1, CV_8UC3, cv::Scalar::all(0))),
                 std::invalid_argument);
}

// A live frame is drawn without a copy: the QImage uses the Mat's pixels and keeps them alive until it goes.
TEST(QImageConversion, WrapAsQImageSharesAndOutlivesTheMat) {
    cv::Mat rgba(2, 3, CV_8UC4, cv::Scalar(10, 20, 30, 255));
    const cv::Mat watcher = rgba; // shares the pixels, to see who else holds them
    {
        const QImage image = qt_common::wrapAsQImage(rgba);
        EXPECT_EQ(image.constBits(), rgba.data);
        rgba.release();

        EXPECT_EQ(watcher.u->refcount, 2);
        EXPECT_EQ(image.format(), QImage::Format_RGBA8888);
        EXPECT_EQ(image.size(), QSize(3, 2));
        EXPECT_EQ(image.pixelColor(2, 1), QColor(10, 20, 30, 255));
    }
    EXPECT_EQ(watcher.u->refcount, 1);
}

} // namespace
