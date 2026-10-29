#include "thumbnail_delegate.h"

#include "display_text.h"
#include "outline_icons.h"
#include "shades.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QScrollBar>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QToolTip>

#include <algorithm>
#include <cstddef>

namespace {

constexpr int kPadding = 6;
constexpr int kIconColumnWidth = 28;
constexpr int kCaptionGap = 4;
constexpr int kIconSize = 20;
constexpr int kIconGap = 4;
// Top to bottom in the icon column, left to right in a compact row; the order of ThumbnailDelegate::m_icons.
constexpr std::array<ThumbnailAction, 3> kIconActions = {ThumbnailAction::Rename, ThumbnailAction::Duplicate,
                                                         ThumbnailAction::Delete};
constexpr int kIconRowWidth = 3 * kIconSize + 2 * kIconGap;
constexpr int kFallbackCellWidth = 200; // for a view that is not a scroll area, which the sidebar never is

// The width every cell gets: the viewport's width as it is while the vertical scroll bar shows. The cell's
// height follows from its width, so a width that changed as the scroll bar came and went could take away the
// reason for the scroll bar, and bring it back again. Overlay scroll bars, as on macOS, take no room.
int cellWidth(const QWidget* widget) {
    const auto* view = qobject_cast<const QAbstractScrollArea*>(widget);
    if (view == nullptr) {
        return kFallbackCellWidth;
    }
    int width = view->maximumViewportSize().width();
    const QScrollBar* bar = view->verticalScrollBar();
    if (view->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded &&
        view->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, bar) == 0) {
        width -= view->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, bar);
    }
    return std::max(width, 1);
}

bool showsThumbnail(SidebarFields fields) { return fields.testFlag(SidebarField::Thumbnail); }

int captionLineCount(SidebarFields fields) {
    return (fields.testFlag(SidebarField::Name) ? 1 : 0) + (fields.testFlag(SidebarField::Details) ? 1 : 0);
}

// Below the picture box: the gap and the caption lines, or nothing when no caption is shown.
int captionHeight(SidebarFields fields, int lineSpacing) {
    const int lines = captionLineCount(fields);
    return lines > 0 ? kCaptionGap + lines * lineSpacing : 0;
}

// The list stretches a cell to the viewport's width when there is no scroll bar, so the boxes are measured
// from the cell as drawn rather than from the size hint.
QRect pictureBox(const QRect& cell, SidebarFields fields, int lineSpacing) {
    const int width = std::max(1, cell.width() - 2 * kPadding - kIconColumnWidth);
    const int height = std::max(1, cell.height() - 2 * kPadding - captionHeight(fields, lineSpacing));
    return {cell.left() + kPadding, cell.top() + kPadding, width, height};
}

// Under the picture, or, in a compact row, left of the icons. The lines sit in the middle of the box.
QRect captionBox(const QRect& cell, SidebarFields fields, int lineSpacing) {
    const int height = captionLineCount(fields) * lineSpacing;
    if (showsThumbnail(fields)) {
        return {cell.left() + kPadding, cell.bottom() + 1 - kPadding - height,
                std::max(1, cell.width() - 2 * kPadding), height};
    }
    return {cell.left() + kPadding, cell.top() + (cell.height() - height) / 2,
            std::max(1, cell.width() - 3 * kPadding - kIconRowWidth), height};
}

int iconSlot(ThumbnailAction action) {
    const auto found = std::find(kIconActions.begin(), kIconActions.end(), action);
    return found == kIconActions.end() ? -1 : static_cast<int>(found - kIconActions.begin());
}

QString iconToolTip(ThumbnailAction action) {
    const char* name = "";
    switch (action) {
    case ThumbnailAction::Rename:
        name = QT_TRANSLATE_NOOP("ThumbnailDelegate", "Rename");
        break;
    case ThumbnailAction::Duplicate:
        name = QT_TRANSLATE_NOOP("ThumbnailDelegate", "Duplicate");
        break;
    case ThumbnailAction::Delete:
        name = QT_TRANSLATE_NOOP("ThumbnailDelegate", "Delete");
        break;
    case ThumbnailAction::CopyImage:
    case ThumbnailAction::SaveAs:
        break;
    }
    return QStringLiteral("%1 (%2)").arg(QCoreApplication::translate("ThumbnailDelegate", name),
                                         thumbnailActionShortcut(action).toString(QKeySequence::NativeText));
}

viewer_core::ImageId idOf(const QModelIndex& index) {
    return index.data(ThumbnailModel::IdRole).value<quint64>();
}

} // namespace

ThumbnailDelegate::ThumbnailDelegate(const ThumbnailModel& model, QObject* parent)
    : QStyledItemDelegate(parent), m_model(model),
      m_icons{qt_common::makeOutlineIcon(qt_common::OutlineIcon::Pencil),
              qt_common::makeOutlineIcon(qt_common::OutlineIcon::Duplicate),
              qt_common::makeOutlineIcon(qt_common::OutlineIcon::Trash)} {}

QRect ThumbnailDelegate::iconRect(const QRect& cell, const QFontMetrics& metrics, SidebarFields fields,
                                  ThumbnailAction action) {
    const int slot = iconSlot(action);
    if (slot < 0) {
        return {};
    }
    if (!showsThumbnail(fields)) {
        const int left = cell.right() + 1 - kPadding - kIconRowWidth + slot * (kIconSize + kIconGap);
        return {left, cell.top() + (cell.height() - kIconSize) / 2, kIconSize, kIconSize};
    }
    const QRect box = pictureBox(cell, fields, metrics.lineSpacing());
    // A narrow sidebar has a short box: the icons shrink so all three fit beside it.
    const int size = std::clamp((box.height() - 2 * kIconGap) / 3, 1, kIconSize);
    return {box.right() + 1 + (kIconColumnWidth - size) / 2, box.top() + slot * (size + kIconGap), size,
            size};
}

bool ThumbnailDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                    const QStyleOptionViewItem& option, const QModelIndex& index) {
    const QEvent::Type type = event->type();
    if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease ||
        type == QEvent::MouseButtonDblClick) {
        const auto* mouse = static_cast<const QMouseEvent*>(event);
        for (const ThumbnailAction action : kIconActions) {
            if (mouse->button() == Qt::LeftButton &&
                iconRect(option.rect, option.fontMetrics, m_model.shownFields(), action)
                    .contains(mouse->position().toPoint())) {
                // Acts on release, like a button; the press and a double click stop here so they neither
                // start a drag nor activate the item.
                if (type == QEvent::MouseButtonRelease) {
                    emit actionRequested(idOf(index), action);
                }
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool ThumbnailDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
                                  const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::ToolTip) {
        for (const ThumbnailAction action : kIconActions) {
            if (iconRect(option.rect, option.fontMetrics, m_model.shownFields(), action)
                    .contains(event->pos())) {
                QToolTip::showText(event->globalPos(), iconToolTip(action), view);
                return true;
            }
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

QRect ThumbnailDelegate::pictureRect(const QRect& cell, const QFontMetrics& metrics, SidebarFields fields,
                                     QSize imageSize) {
    if (imageSize.isEmpty() || !showsThumbnail(fields)) {
        return {};
    }
    const QRect box = pictureBox(cell, fields, metrics.lineSpacing());
    const double scale = std::min(static_cast<double>(box.width()) / imageSize.width(),
                                  static_cast<double>(box.height()) / imageSize.height());
    const QSize size(std::max(1, qRound(imageSize.width() * scale)),
                     std::max(1, qRound(imageSize.height() * scale)));
    return QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size, box);
}

void ThumbnailDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    QStyleOptionViewItem cell = option;
    initStyleOption(&cell, index);
    const bool selected = cell.state.testFlag(QStyle::State_Selected);

    painter->save();
    // Greys rather than the accent colour, read at paint time so a change of appearance is followed. The
    // selection keeps its grey without focus, so the shown image stays marked while the owner works
    // elsewhere; focus only strengthens it.
    const QPalette palette = QGuiApplication::palette();
    if (selected) {
        const bool focused = cell.widget != nullptr && cell.widget->hasFocus();
        painter->fillRect(cell.rect, qt_common::selectionGrey(palette, focused));
    } else if (cell.state.testFlag(QStyle::State_MouseOver)) {
        painter->fillRect(cell.rect, qt_common::hoverGrey(palette));
    }
    if (index.row() + 1 < index.model()->rowCount(index.parent())) {
        painter->fillRect(cell.rect.left(), cell.rect.bottom(), cell.rect.width(), 1,
                          qt_common::dividerGrey(palette));
    }

    const SidebarFields fields = m_model.shownFields();
    // No thumbnail is made while the thumbnail is hidden.
    if (showsThumbnail(fields)) {
        const QRect picture = pictureRect(cell.rect, cell.fontMetrics, fields,
                                          index.data(ThumbnailModel::ImageSizeRole).toSize());
        const qreal ratio = painter->device()->devicePixelRatio();
        const QSize deviceSize(qRound(picture.width() * ratio), qRound(picture.height() * ratio));
        painter->drawPixmap(picture, m_model.thumbnail(idOf(index), deviceSize));
    }

    if (selected || cell.state.testFlag(QStyle::State_MouseOver)) {
        for (std::size_t slot = 0; slot < kIconActions.size(); ++slot) {
            m_icons[slot].paint(painter, iconRect(cell.rect, cell.fontMetrics, fields, kIconActions[slot]),
                                Qt::AlignCenter, QIcon::Normal);
        }
    }

    QStringList texts;
    if (fields.testFlag(SidebarField::Name)) {
        texts << index.data(ThumbnailModel::NameRole).toString();
    }
    if (fields.testFlag(SidebarField::Details)) {
        texts << index.data(ThumbnailModel::DetailsRole).toString();
    }
    const int lineSpacing = cell.fontMetrics.lineSpacing();
    QRect line = captionBox(cell.rect, fields, lineSpacing);
    line.setHeight(lineSpacing);
    const Qt::Alignment alignment = showsThumbnail(fields) ? Qt::AlignHCenter : Qt::AlignLeft;
    painter->setFont(cell.font);
    painter->setPen(cell.palette.color(QPalette::Text));
    for (const QString& text : texts) {
        painter->drawText(line, alignment | Qt::AlignTop | Qt::TextSingleLine,
                          cell.fontMetrics.elidedText(singleLine(text), Qt::ElideMiddle, line.width()));
        line.translate(0, lineSpacing);
    }
    painter->restore();
}

QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const {
    const int width = cellWidth(option.widget);
    const SidebarFields fields = m_model.shownFields();
    const int lineSpacing = option.fontMetrics.lineSpacing();
    if (!showsThumbnail(fields)) {
        return {width, 2 * kPadding + std::max(captionLineCount(fields) * lineSpacing, kIconSize)};
    }
    const int boxWidth = std::max(1, width - 2 * kPadding - kIconColumnWidth);
    const int boxHeight = std::max(1, qRound(boxWidth * 3.0 / 4.0));
    return {width, 2 * kPadding + boxHeight + captionHeight(fields, lineSpacing)};
}
