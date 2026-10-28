#include "display_text.h"

#include <QCoreApplication>
#include <QDateTime>

#include <cstdint>

QString imageDetails(const viewer_core::ReceivedImage& image, const QLocale& locale) {
    const auto size = [&locale](std::uint64_t bytes) {
        return locale.formattedDataSize(static_cast<qint64>(bytes), 1, QLocale::DataSizeSIFormat);
    };
    const QString summary = QString::fromStdString(image.summary);
    if (image.origin.source == viewer_core::ImageSource::OpenedFile && image.origin.fileBytes) {
        return QCoreApplication::translate("ImageInfoBar", "%1 · %2 file")
            .arg(summary, size(*image.origin.fileBytes));
    }
    return QStringLiteral("%1 · %2").arg(summary, size(image.pixelDataBytes()));
}

QString clockTime(qint64 epochMicroseconds) {
    return QDateTime::fromMSecsSinceEpoch(epochMicroseconds / 1000).toString(QStringLiteral("HH:mm:ss"));
}
