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

QString liveDetails(const QString& summary, const viewer_core::LiveChannel& channel,
                    const viewer_core::LiveStats& stats) {
    const QString state = channel.frozen ? QCoreApplication::translate("ImageInfoBar", "frozen")
                          : channel.idle ? QCoreApplication::translate("ImageInfoBar", "idle")
                                         : QCoreApplication::translate("ImageInfoBar", "live");
    QString text = QCoreApplication::translate("ImageInfoBar", "%1 · %2 · %3 fps in · %4 drawn · %5 dropped")
                       .arg(state, summary)
                       .arg(stats.arrivedPerSecond)
                       .arg(stats.drawnPerSecond)
                       .arg(channel.dropped);
    if (stats.age) {
        text += QCoreApplication::translate("ImageInfoBar", " · %1 ms").arg(stats.age->count());
    }
    return text;
}

QString clockTime(qint64 epochMicroseconds) {
    return QDateTime::fromMSecsSinceEpoch(epochMicroseconds / 1000).toString(QStringLiteral("HH:mm:ss"));
}
