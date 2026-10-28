#pragma once

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QWidget>

namespace test_support {

// Puts the application palette back however the test ends.
class PaletteGuard {
public:
    PaletteGuard() : m_saved(QGuiApplication::palette()) {}
    ~PaletteGuard() { QGuiApplication::setPalette(m_saved); }
    PaletteGuard(const PaletteGuard&) = delete;
    PaletteGuard& operator=(const PaletteGuard&) = delete;

private:
    QPalette m_saved;
};

// Sets the application palette and tells every window, as Qt does only once the event loop runs (the tests
// never start it). As in the running viewer when the appearance changes, a window passes the change on to its
// children as a PaletteChange: they never see the ApplicationPaletteChange.
inline void applyPalette(const QPalette& palette) {
    QGuiApplication::setPalette(palette);
    QEvent changed(QEvent::ApplicationPaletteChange);
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        QCoreApplication::sendEvent(widget, &changed);
    }
}

// A palette with these window and text colours, the two every shade is blended from.
inline QPalette paletteWith(const QColor& window, const QColor& text) {
    QPalette palette = QGuiApplication::palette();
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    return palette;
}

} // namespace test_support
