#pragma once

#include "viewer_core/image_history.h"
#include "viewer_core/live_channels.h"

#include <QChar>
#include <QLocale>
#include <QString>
#include <QtGlobal>

// A name may hold line breaks, but the sidebar and the info strip show it on one line: each break becomes a
// space.
inline QString singleLine(QString text) {
    for (QChar& character : text) {
        if (character == u'\n' || character == u'\r' || character == QChar::LineSeparator ||
            character == QChar::ParagraphSeparator) {
            character = u' ';
        }
    }
    return text;
}

// "640×480 · 1 ch · 16S · 614.4 kB": the summary, then the size of the pixel data for a received image, or
// the size on disk followed by " file" for an opened one. Decimal units, as Finder shows them. The info strip
// and the sidebar's Details line both show it.
QString imageDetails(const viewer_core::ReceivedImage& image, const QLocale& locale = QLocale());

// "live · 1920×1080 · 3 ch · 8U · 30 fps in · 30 drawn · 12 dropped · 18 ms": the state ("frozen" before
// "idle" before "live"), the summary, frames arrived and drawn in the last second, frames the viewer dropped,
// and the shown frame's age, left out without a sentAt. The info strip shows it for a live channel.
QString liveDetails(const QString& summary, const viewer_core::LiveChannel& channel,
                    const viewer_core::LiveStats& stats);

// "camera 14:03:22.517": a snapshot's name, from its frame's receive time, local and to the millisecond.
QString snapshotName(const QString& channel, qint64 epochMilliseconds);

// "14:05:09": local time to the second, as the log line and the thumbnails' tool tips show it.
QString clockTime(qint64 epochMicroseconds);
