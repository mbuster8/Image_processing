#include "shades.h"

#include <QEvent>
#include <QGuiApplication>
#include <QObject>
#include <QPainter>
#include <QPalette>

namespace qt_common {
namespace {

// How far each shade lies from the window colour towards the text colour.
constexpr qreal kPanel = 0.05;
constexpr qreal kHover = 0.11;
constexpr qreal kSelection = 0.18;
constexpr qreal kFocusedSelection = 0.26;
constexpr qreal kDivider = 0.22;

QColor towardsText(const QPalette& palette, qreal amount) {
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    const auto mix = [amount](int from, int to) { return qRound(from + (to - from) * amount); };
    return QColor(mix(window.red(), text.red()), mix(window.green(), text.green()),
                  mix(window.blue(), text.blue()));
}

void applyPanelGrey(QWidget* widget) {
    QPalette palette = widget->palette();
    const QColor grey = panelGrey(QGuiApplication::palette());
    if (palette.color(QPalette::Window) == grey && palette.color(QPalette::Base) == grey) {
        return;
    }
    palette.setColor(QPalette::Window, grey);
    palette.setColor(QPalette::Base, grey);
    widget->setPalette(palette);
}

// Qt keeps a role set on a widget through a change of the application palette, so the grey is worked out
// again from the new one. The change reaches a panel only as a PaletteChange passed down from its window,
// the event setting the grey sends too: that one finds the grey already in place and ends there.
class PanelGreyFollower : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::PaletteChange) {
            applyPanelGrey(static_cast<QWidget*>(watched));
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

QColor panelGrey(const QPalette& palette) { return towardsText(palette, kPanel); }

QColor hoverGrey(const QPalette& palette) { return towardsText(palette, kHover); }

QColor selectionGrey(const QPalette& palette, bool focused) {
    return towardsText(palette, focused ? kFocusedSelection : kSelection);
}

QColor dividerGrey(const QPalette& palette) { return towardsText(palette, kDivider); }

void usePanelGrey(QWidget* widget) {
    widget->setAutoFillBackground(true);
    applyPanelGrey(widget);
    widget->installEventFilter(new PanelGreyFollower(widget));
}

DividerLine::DividerLine(QWidget* parent) : QWidget(parent) {
    setFixedHeight(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void DividerLine::paintEvent(QPaintEvent* /*event*/) {
    QPainter(this).fillRect(rect(), dividerGrey(QGuiApplication::palette()));
}

} // namespace qt_common
