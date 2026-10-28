#include "eliding_label.h"

#include <QPainter>

ElidingLabel::ElidingLabel(Qt::TextElideMode mode, QWidget* parent) : QLabel(parent), m_mode(mode) {
    setTextFormat(Qt::PlainText);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QSize ElidingLabel::minimumSizeHint() const {
    return {fontMetrics().horizontalAdvance(QStringLiteral("…")), QLabel::minimumSizeHint().height()};
}

void ElidingLabel::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setPen(palette().color(foregroundRole()));
    const QRect area = contentsRect();
    painter.drawText(area, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     fontMetrics().elidedText(text(), m_mode, area.width()));
}
