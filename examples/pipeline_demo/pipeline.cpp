#include "pipeline.h"

#include <opencv2/imgproc.hpp>
// OpenCV 5 moved the distance types (cv::DIST_L2) from imgproc into the geometry module; OpenCV 4 has no
// such header.
#if __has_include(<opencv2/geometry.hpp>)
#include <opencv2/geometry.hpp>
#endif

#include <algorithm>
#include <vector>

namespace pipeline_demo {

namespace {

cv::Mat toBgr(const cv::Mat& original) {
    if (original.channels() == 3) {
        return original;
    }
    cv::Mat bgr;
    cv::cvtColor(original, bgr, original.channels() == 4 ? cv::COLOR_BGRA2BGR : cv::COLOR_GRAY2BGR);
    return bgr;
}

cv::Mat toGrey(const cv::Mat& original) {
    if (original.channels() == 1) {
        return original;
    }
    cv::Mat grey;
    cv::cvtColor(original, grey, original.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
    return grey;
}

cv::Mat edges(const cv::Mat& original) {
    cv::Mat result;
    cv::Canny(toGrey(original), result, 50, 150);
    return result;
}

// 0 at the left edge to 255 at the right: shows the viewer's alpha handling.
cv::Mat withAlphaGradient(const cv::Mat& original) {
    const cv::Mat bgr = toBgr(original);
    cv::Mat row(1, bgr.cols, CV_8UC1);
    for (int x = 0; x < bgr.cols; ++x) {
        row.at<uchar>(0, x) = cv::saturate_cast<uchar>(255.0 * x / std::max(1, bgr.cols - 1));
    }
    cv::Mat alpha;
    cv::repeat(row, bgr.rows, 1, alpha);
    std::vector<cv::Mat> channels;
    cv::split(bgr, channels);
    channels.push_back(alpha);
    cv::Mat bgra;
    cv::merge(channels, bgra);
    return bgra;
}

// Euclidean distance of every pixel to the nearest Canny edge: fractional values in 32-bit floating point.
cv::Mat distanceToEdges(const cv::Mat& original) {
    const cv::Mat notEdges = edges(original) == 0;
    cv::Mat distance;
    cv::distanceTransform(notEdges, distance, cv::DIST_L2, cv::DIST_MASK_PRECISE, CV_32F);
    return distance;
}

// A region of interest shares the original's pixels and skips the rest of each row, so it is not
// continuous; the library must send it correctly anyway.
cv::Mat centreCrop(const cv::Mat& original) {
    const cv::Mat bgr = toBgr(original);
    return bgr(cv::Rect(bgr.cols / 4, bgr.rows / 4, std::max(1, bgr.cols / 2), std::max(1, bgr.rows / 2)));
}

} // namespace

std::vector<PipelineStage> buildPipeline() {
    return {
        {"Original", [](const cv::Mat& original) { return toBgr(original); }},
        {"With alpha", withAlphaGradient},
        {"Greyscale", [](const cv::Mat& original) { return toGrey(original); }},
        {"Gaussian blur",
         [](const cv::Mat& original) {
             cv::Mat result;
             cv::GaussianBlur(toGrey(original), result, {9, 9}, 0);
             return result;
         }},
        {"Sobel X",
         [](const cv::Mat& original) {
             cv::Mat result;
             cv::Sobel(toGrey(original), result, CV_16S, 1, 0);
             return result;
         }},
        {"Canny edges", edges},
        {"Distance transform", distanceToEdges},
        {"Laplacian",
         [](const cv::Mat& original) {
             cv::Mat result;
             cv::Laplacian(toGrey(original), result, CV_64F);
             return result;
         }},
        {"Centre crop", centreCrop},
        {"Multi-channel 16-bit",
         [](const cv::Mat& original) {
             cv::Mat result;
             toBgr(original).convertTo(result, CV_16UC3, 257.0); // 255 × 257 = 65535: the full 16-bit range
             return result;
         }},
    };
}

cv::Mat makeTestPattern(cv::Size size) {
    cv::Mat pattern(size, CV_8UC3);
    const double xSpan = std::max(1, size.width - 1);
    const double ySpan = std::max(1, size.height - 1);
    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            pattern.at<cv::Vec3b>(y, x) = cv::Vec3b(cv::saturate_cast<uchar>(255.0 * x / xSpan),
                                                    cv::saturate_cast<uchar>(255.0 * y / ySpan), 96);
        }
    }

    const int shortSide = std::min(size.width, size.height);
    cv::circle(pattern, {size.width / 4, size.height / 2}, shortSide / 6, cv::Scalar(255, 255, 255), cv::FILLED,
               cv::LINE_AA);
    cv::rectangle(pattern, cv::Rect(size.width / 2, size.height / 4, size.width / 4, size.height / 3),
                  cv::Scalar(0, 0, 255), cv::FILLED);
    cv::line(pattern, {0, size.height - 1}, {size.width - 1, 0}, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
    cv::putText(pattern, "visual_debug", {size.width / 16, size.height - size.height / 10},
                cv::FONT_HERSHEY_SIMPLEX, 1.5 * size.width / 640.0, cv::Scalar(255, 255, 255), 3, cv::LINE_AA);
    return pattern;
}

} // namespace pipeline_demo
