#include "qimage_conversion.h"

#include <stdexcept>

namespace qt_common {

QImage toQImage(const cv::Mat& rgba) {
    if (rgba.type() != CV_8UC4) {
        throw std::invalid_argument("toQImage expects an 8-bit, four-channel image");
    }
    // The wrapping QImage borrows the Mat's pixels; copy() gives the result its own.
    return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<qsizetype>(rgba.step), QImage::Format_RGBA8888)
        .copy();
}

} // namespace qt_common
