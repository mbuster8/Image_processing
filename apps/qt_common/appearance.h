#pragma once

class QMenu;

namespace qt_common {

// Whether the programs follow the system's light or dark appearance, or keep one. The viewer and the example
// sender share the choice: it lives in the viewer's settings, whichever program makes it.
enum class Appearance { System, Light, Dark };

bool canChooseAppearance();    // true when built with Qt 6.8 or newer
Appearance storedAppearance(); // System when unset or unrecognised
void storeAppearance(Appearance appearance);
void applyAppearance(Appearance appearance); // does nothing before Qt 6.8

// Adds View → Appearance with three exclusive checkable actions ("System", "Light", "Dark"), after a
// separator when the menu already holds entries. It keeps them, the setting and the application in step, and
// re-applies the stored choice whenever the application becomes active, so a choice made in the other
// program arrives. Returns nullptr and adds nothing when canChooseAppearance() is false.
QMenu* addAppearanceMenu(QMenu* viewMenu);

} // namespace qt_common
