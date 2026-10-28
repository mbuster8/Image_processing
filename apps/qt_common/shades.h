#pragma once

#include <QColor>
#include <QWidget>

class QPaintEvent;
class QPalette;

namespace qt_common {

// Greys blended from the palette's window colour towards its text colour, so they suit a light and a dark
// appearance alike, each a step further from the window colour than the one before. Pass the application
// palette: a panel's own palette has its window colour replaced by the panel grey.
QColor panelGrey(const QPalette& palette); // the sidebar, the log, the status bar, the sender's command bar
QColor hoverGrey(const QPalette& palette); // a cell under the pointer
QColor selectionGrey(const QPalette& palette, bool focused);
QColor dividerGrey(const QPalette& palette); // lines between panels and between cells

// Fills the widget, and the lists in it, with the panel grey, again whenever the application palette
// changes, so a switch of appearance shows at once.
void usePanelGrey(QWidget* widget);

// A 1-point horizontal line in the divider grey, as wide as its layout makes it.
class DividerLine : public QWidget {
public:
    explicit DividerLine(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace qt_common
