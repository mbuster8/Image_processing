#pragma once

#include <QKeySequence>

// What can be done to one image from the sidebar. Only Rename, Duplicate and Delete have icons in the cell:
// five would not fit at small widths.
enum class ThumbnailAction { Rename, Duplicate, Delete, CopyImage, SaveAs };

// The shortcut the Edit menu gives the action; empty for one it has none.
inline QKeySequence thumbnailActionShortcut(ThumbnailAction action) {
    switch (action) {
    case ThumbnailAction::Rename:
        return QKeySequence(Qt::CTRL | Qt::Key_R);
    case ThumbnailAction::Duplicate:
        return QKeySequence(Qt::CTRL | Qt::Key_D);
    case ThumbnailAction::Delete:
        return QKeySequence(Qt::CTRL | Qt::Key_Backspace);
    case ThumbnailAction::CopyImage:
        return QKeySequence(QKeySequence::Copy);
    case ThumbnailAction::SaveAs:
        break;
    }
    return {};
}
