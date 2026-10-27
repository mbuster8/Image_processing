#pragma once

#include <QString>

#include <opencv2/core/mat.hpp>

#include <variant>

class QWidget;

namespace qt_common {

// Asks for an image file, starting in the user's Pictures folder. Returns an empty string if cancelled.
QString chooseImageFile(QWidget* parent);

// Reads an image keeping its depth and channels, so a 16-bit PNG stays 16-bit. Returns a message instead when
// the file cannot be read or holds an image visual_debug cannot carry (for example more than four channels).
std::variant<cv::Mat, QString> readImageFile(const QString& path);

} // namespace qt_common
