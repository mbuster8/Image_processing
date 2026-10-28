#pragma once

#include "viewer_core/image_history.h"

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

// "14:05:09": local time to the second, as the log line and the thumbnails' tool tips show it.
QString clockTime(qint64 epochMicroseconds);
