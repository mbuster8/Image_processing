#pragma once

#include <QImage>

#include <opencv2/core/mat.hpp>

namespace qt_common {

// Copies an 8-bit RGBA image (CV_8UC4, as viewer_core::toDisplayImage returns) into a QImage, so the QImage
// stays valid after the Mat changes or goes away. Throws std::invalid_argument for any other type.
QImage toQImage(const cv::Mat& rgba);

} // namespace qt_common
