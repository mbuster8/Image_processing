#include "controls.h"

#include <QApplication>
#include <QSize>
#include <QToolButton>
#include <QWidget>

namespace qt_common {

// The application font usually has every property marked already; a font without the marks would leave a
// widget with the font of its class.
QFont regularFont() {
    QFont font = QApplication::font();
    font.setResolveMask(QFont::AllPropertiesResolved);
    return font;
}

void sizeControl(QWidget* control) { control->setMinimumHeight(kControlHeight); }

void frameButton(QToolButton* button) {
    button->setAutoRaise(false);
    button->setIconSize(QSize(kControlIconSize, kControlIconSize));
    sizeControl(button);
}

} // namespace qt_common
