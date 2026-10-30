#include "outline_icons.h"

#include <QGuiApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>

#include <algorithm>
#include <initializer_list>

namespace qt_common {
namespace {

// Every shape is drawn on a 24×24 grid with a 1.5-unit stroke, round caps and round joins.
constexpr qreal kGrid = 24.0;
constexpr qreal kStroke = 1.5;
constexpr qreal kDotRadius = 0.4; // stroked, so a dot about two units across

void addPolyline(QPainterPath& path, std::initializer_list<QPointF> points, bool closed = false) {
    auto point = points.begin();
    path.moveTo(*point);
    for (++point; point != points.end(); ++point) {
        path.lineTo(*point);
    }
    if (closed) {
        path.closeSubpath();
    }
}

void addDot(QPainterPath& path, QPointF centre) { path.addEllipse(centre, kDotRadius, kDotRadius); }

// One horizontal slider: a track broken by a round knob.
void addSlider(QPainterPath& path, qreal y, qreal knobX) {
    constexpr qreal kKnobRadius = 2.0;
    addPolyline(path, {{4.0, y}, {knobX - kKnobRadius, y}});
    path.addEllipse(QPointF(knobX, y), kKnobRadius, kKnobRadius);
    addPolyline(path, {{knobX + kKnobRadius, y}, {20.0, y}});
}

QPainterPath shapePath(OutlineIcon shape) {
    QPainterPath path;
    switch (shape) {
    case OutlineIcon::Pencil:
        addPolyline(path, {{15.0, 4.5}, {19.5, 9.0}, {9.0, 19.5}, {4.5, 19.5}, {4.5, 15.0}}, true);
        addPolyline(path, {{12.75, 6.75}, {17.25, 11.25}});
        break;
    case OutlineIcon::Duplicate:
        // The back square shows only where the front one does not cover it.
        path.moveTo(8.5, 8.5);
        path.lineTo(8.5, 5.5);
        path.quadTo(8.5, 3.5, 10.5, 3.5);
        path.lineTo(18.5, 3.5);
        path.quadTo(20.5, 3.5, 20.5, 5.5);
        path.lineTo(20.5, 13.5);
        path.quadTo(20.5, 15.5, 18.5, 15.5);
        path.lineTo(15.5, 15.5);
        path.addRoundedRect(QRectF(3.5, 8.5, 12.0, 12.0), 2.0, 2.0);
        break;
    case OutlineIcon::Trash:
        addPolyline(path, {{4.5, 6.5}, {19.5, 6.5}});
        addPolyline(path, {{9.5, 6.5}, {9.5, 4.0}, {14.5, 4.0}, {14.5, 6.5}});
        addPolyline(path, {{6.25, 6.5}, {7.25, 20.0}, {16.75, 20.0}, {17.75, 6.5}});
        addPolyline(path, {{10.25, 10.0}, {10.25, 16.5}});
        addPolyline(path, {{13.75, 10.0}, {13.75, 16.5}});
        break;
    case OutlineIcon::Pause:
        addPolyline(path, {{9.0, 5.5}, {9.0, 18.5}});
        addPolyline(path, {{15.0, 5.5}, {15.0, 18.5}});
        break;
    case OutlineIcon::Play:
        addPolyline(path, {{7.5, 5.0}, {18.5, 12.0}, {7.5, 19.0}}, true);
        break;
    case OutlineIcon::Stop:
        path.addRoundedRect(QRectF(6.0, 6.0, 12.0, 12.0), 1.5, 1.5);
        break;
    case OutlineIcon::ChevronUp:
        addPolyline(path, {{5.5, 15.0}, {12.0, 8.5}, {18.5, 15.0}});
        break;
    case OutlineIcon::ChevronDown:
        addPolyline(path, {{5.5, 9.0}, {12.0, 15.5}, {18.5, 9.0}});
        break;
    case OutlineIcon::Copy:
        // A clipboard: the board, and the clip across its top edge.
        path.addRoundedRect(QRectF(5.5, 5.0, 13.0, 15.5), 1.5, 1.5);
        path.addRoundedRect(QRectF(9.0, 3.5, 6.0, 3.0), 1.0, 1.0);
        break;
    case OutlineIcon::Clear:
        addPolyline(path, {{6.5, 6.5}, {17.5, 17.5}});
        addPolyline(path, {{17.5, 6.5}, {6.5, 17.5}});
        break;
    case OutlineIcon::Search:
        path.addEllipse(QPointF(10.5, 10.5), 6.0, 6.0);
        addPolyline(path, {{15.0, 15.0}, {20.0, 20.0}});
        break;
    case OutlineIcon::Options:
        addSlider(path, 6.0, 15.0);
        addSlider(path, 12.0, 8.5);
        addSlider(path, 18.0, 13.0);
        break;
    case OutlineIcon::Info:
        path.addEllipse(QPointF(12.0, 12.0), 9.0, 9.0);
        addDot(path, {12.0, 8.0});
        addPolyline(path, {{12.0, 11.0}, {12.0, 16.5}});
        break;
    case OutlineIcon::Warning:
        addPolyline(path, {{12.0, 3.75}, {20.75, 19.5}, {3.25, 19.5}}, true);
        addPolyline(path, {{12.0, 9.5}, {12.0, 13.5}});
        addDot(path, {12.0, 16.5});
        break;
    case OutlineIcon::Error:
        // An octagon, like a stop sign, so it differs from Info's circle at a glance.
        addPolyline(path,
                    {{8.6, 3.75},
                     {15.4, 3.75},
                     {20.25, 8.6},
                     {20.25, 15.4},
                     {15.4, 20.25},
                     {8.6, 20.25},
                     {3.75, 15.4},
                     {3.75, 8.6}},
                    true);
        addPolyline(path, {{12.0, 7.5}, {12.0, 13.0}});
        addDot(path, {12.0, 16.5});
        break;
    case OutlineIcon::Folder:
        // A tab on the back, and the open front.
        addPolyline(path, {{3.5, 18.0}, {3.5, 6.0}, {9.0, 6.0}, {11.0, 8.0}, {18.5, 8.0}, {18.5, 10.5}});
        addPolyline(path, {{3.5, 18.0}, {6.5, 10.5}, {21.0, 10.5}, {18.0, 18.0}}, true);
        break;
    case OutlineIcon::Send:
        // A paper plane, flying right.
        addPolyline(path, {{4.0, 4.5}, {20.5, 12.0}, {4.0, 19.5}, {6.75, 12.0}}, true);
        addPolyline(path, {{6.75, 12.0}, {12.5, 12.0}});
        break;
    }
    return path;
}

QColor penColour(QIcon::Mode mode) {
    const QPalette palette = QGuiApplication::palette();
    switch (mode) {
    case QIcon::Disabled:
        return palette.color(QPalette::PlaceholderText);
    case QIcon::Selected:
        return palette.color(QPalette::HighlightedText);
    case QIcon::Normal:
    case QIcon::Active:
        break;
    }
    return palette.color(QPalette::WindowText);
}

class OutlineIconEngine : public QIconEngine {
public:
    explicit OutlineIconEngine(OutlineIcon shape) : m_shape(shape) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State /*state*/) override {
        const qreal side = std::min(rect.width(), rect.height());
        if (side <= 0) {
            return;
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0);
        painter->scale(side / kGrid, side / kGrid);
        painter->strokePath(shapePath(m_shape),
                            QPen(penColour(mode), kStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->restore();
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pixmap;
    }

    QIconEngine* clone() const override { return new OutlineIconEngine(m_shape); }

private:
    OutlineIcon m_shape;
};

} // namespace

QIcon makeOutlineIcon(OutlineIcon shape) { return QIcon(new OutlineIconEngine(shape)); }

} // namespace qt_common
