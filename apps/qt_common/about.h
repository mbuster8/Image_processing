#pragma once

#include <QString>

class QMenu;
class QMenuBar;

namespace qt_common {

// The About box's text, as rich text: the application's name and version, the description, then the OpenCV
// and Qt versions the program runs with, so it is plain what the program is built on.
QString aboutText(const QString& description);

// Adds Help with "About <application name>", which shows aboutText(description). The action has the About
// role, so macOS moves it into the application menu, where Mac users look for it.
QMenu* addHelpMenu(QMenuBar* menuBar, const QString& description);

} // namespace qt_common
