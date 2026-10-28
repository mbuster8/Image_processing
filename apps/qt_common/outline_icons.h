#pragma once

#include <QIcon>

namespace qt_common {

// The applications' icons: original outline drawings in the style of Heroicons, stroked in code so the build
// needs no Qt SVG module and no third-party icon licence.
enum class OutlineIcon {
    Pencil,
    Duplicate,
    Trash,
    Pause,
    Play,
    Stop,
    ChevronUp,
    ChevronDown,
    Copy,
    Clear,
    Search,
    Options,
    Info,
    Warning,
    Error,
    Folder,
    Send
};

// Strokes the shape at any size, in the application palette's colour at paint time, so the icon follows a
// switch between dark and light: WindowText normally, HighlightedText when selected, PlaceholderText when
// disabled.
QIcon makeOutlineIcon(OutlineIcon shape);

} // namespace qt_common
