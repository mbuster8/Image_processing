#include "divider_splitter.h"

#include "shades.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QRectF>
#include <QSplitterHandle>
#include <QTransform>

namespace {

constexpr qreal kGripLength = 24.0;
constexpr qreal kGripThickness = 3.0;

class DividerHandle : public QSplitterHandle {
public:
    using QSplitterHandle::QSplitterHandle;

protected:
    // Drawn as if the widgets sat side by side, with the painter turned for a handle between rows.
    void paintEvent(QPaintEvent* /*event*/) override {
        const QPalette palette = QGuiApplication::palette();
        QPainter painter(this);
        painter.fillRect(rect(), qt_common::panelGrey(palette));
        const bool sideBySide = orientation() == Qt::Horizontal;
        const int across = sideBySide ? width() : height();
        const int along = sideBySide ? height() : width();
        if (!sideBySide) {
            painter.setTransform(QTransform(0, 1, 1, 0, 0, 0)); // swaps x and y
        }
        const QColor line = qt_common::dividerGrey(palette);
        painter.fillRect(across / 2, 0, 1, along, line);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(line);
        painter.drawRoundedRect(QRectF(across / 2 + 0.5 - kGripThickness / 2, (along - kGripLength) / 2,
                                       kGripThickness, kGripLength),
                                kGripThickness / 2, kGripThickness / 2);
    }
};

} // namespace

DividerSplitter::DividerSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent) {
    setHandleWidth(kHandleWidth);
}

QSplitterHandle* DividerSplitter::createHandle() { return new DividerHandle(orientation(), this); }
