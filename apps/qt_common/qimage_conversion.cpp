#include "qimage_conversion.h"

#include <stdexcept>

namespace qt_common {
namespace {

void checkRgba(const cv::Mat& rgba) {
    if (rgba.type() != CV_8UC4) {
        throw std::invalid_argument("expected an 8-bit, four-channel image");
    }
}

// Borrows the Mat's pixels; the caller copies them or keeps them alive.
QImage borrow(const cv::Mat& rgba) {
    return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<qsizetype>(rgba.step),
                  QImage::Format_RGBA8888);
}

} // namespace

QImage toQImage(const cv::Mat& rgba) {
    checkRgba(rgba);
    return borrow(rgba).copy();
}

QImage wrapAsQImage(cv::Mat rgba) {
    checkRgba(rgba);
    auto* owner = new cv::Mat(std::move(rgba)); // the reference the QImage's cleanup gives back
    return QImage(
        owner->data, owner->cols, owner->rows, static_cast<qsizetype>(owner->step), QImage::Format_RGBA8888,
        [](void* mat) { delete static_cast<cv::Mat*>(mat); }, owner);
}

} // namespace qt_common
