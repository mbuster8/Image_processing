#pragma once

#include <QFont>

class QToolButton;
class QWidget;

namespace qt_common {

// Controls in status bars, toolbars and panel headers share one size, large enough to see and to hit.
inline constexpr int kControlHeight = 28;
inline constexpr int kControlIconSize = 18;

// The application's font, marked as set, so a widget given it keeps it instead of the smaller font some
// platforms (macOS among them) give status bars and tool buttons; the widget's children inherit it.
QFont regularFont();

// At least kControlHeight high.
void sizeControl(QWidget* control);

// Shows its frame, so its extent is visible, with a kControlIconSize icon, at least kControlHeight high.
// Keep it out of a QToolBar: macOS draws a tool bar's buttons without their frames.
void frameButton(QToolButton* button);

} // namespace qt_common
