#pragma once

#include <QImage>

#include <opencv2/core/mat.hpp>

namespace qt_common {

// Copies an 8-bit RGBA image (CV_8UC4, as viewer_core::toDisplayImage returns) into a QImage, so the QImage
// stays valid after the Mat changes or goes away. Throws std::invalid_argument for any other type.
QImage toQImage(const cv::Mat& rgba);

// The same pixels without a copy, for a picture drawn many times a second: the QImage holds a reference to
// the Mat's data until it and its copies are gone. Throws std::invalid_argument for anything but CV_8UC4.
QImage wrapAsQImage(cv::Mat rgba);

} // namespace qt_common
