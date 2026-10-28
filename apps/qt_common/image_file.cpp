#include "image_file.h"

#include "viewer_core/image_formats.h"
#include "visual_debug/wire_format.h"

#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStringList>

#include <opencv2/imgcodecs.hpp>

#include <exception>
#include <limits>
#include <string>

namespace qt_common {

QString imageFileFilter() {
    QStringList patterns;
    for (const std::string_view extension : viewer_core::imageExtensions()) {
        patterns.append(QStringLiteral("*.") + QString::fromStdString(std::string(extension)));
    }
    return QFileDialog::tr("Images (%1);;All files (*)").arg(patterns.join(QLatin1Char(' ')));
}

QString chooseImageFile(QWidget* parent) {
    return QFileDialog::getOpenFileName(parent, QFileDialog::tr("Open Image"),
                                        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                        imageFileFilter());
}

std::variant<cv::Mat, QString> readImageFile(const QString& path) {
    const QString notSupported = QStringLiteral("not a supported image");
    try {
        // Qt opens the file and OpenCV decodes the bytes. cv::imread takes a narrow path, which on Windows
        // means the ANSI code page, so a name outside it would not open (the 2013 code used toAscii).
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return notSupported;
        }
        QByteArray bytes = file.readAll();
        if (bytes.isEmpty() || bytes.size() > std::numeric_limits<int>::max()) {
            return notSupported;
        }
        const cv::Mat encoded(1, static_cast<int>(bytes.size()), CV_8UC1, bytes.data());
        cv::Mat image = cv::imdecode(encoded, cv::IMREAD_UNCHANGED);
        // imdecode returns an empty Mat for data it cannot decode, and decoders can return shapes nothing
        // downstream shows (3-D, more than four channels).
        if (!visual_debug::isSupportedImage(image)) {
            return notSupported;
        }
        return image;
    } catch (const std::exception& error) {
        return QString::fromLocal8Bit(error.what());
    }
}

} // namespace qt_common
