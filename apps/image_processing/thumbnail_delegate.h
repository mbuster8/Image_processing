#pragma once

#include "thumbnail_action.h"
#include "thumbnail_model.h"

#include "viewer_core/image_history.h"

#include <QIcon>
#include <QRect>
#include <QSize>
#include <QStyledItemDelegate>

#include <array>

class QFontMetrics;

// Draws one sidebar cell in the layout the model's shown fields ask for. With the thumbnail: a picture box
// three quarters as high as it is wide, with the image fitted and centred in it; the icon column to its
// right; and a caption line below for each of name and details that is shown. Without it, a compact row: the
// text lines on the left and the three icons in a row on the right. Text is elided in the middle. The size
// comes from the list's width and the shown fields alone, so every cell has the same size. The icons show on
// the selected and the hovered cell, and their place is kept on every cell, so text never moves.
class ThumbnailDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ThumbnailDelegate(const ThumbnailModel& model, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    // Where an image of `imageSize` (columns × rows) is drawn in `cell`, for captions set in `metrics`; empty
    // when the thumbnail is not shown.
    static QRect pictureRect(const QRect& cell, const QFontMetrics& metrics, SidebarFields fields,
                             QSize imageSize);
    // The action's icon in `cell`; empty for an action without one.
    static QRect iconRect(const QRect& cell, const QFontMetrics& metrics, SidebarFields fields,
                          ThumbnailAction action);

signals:
    void actionRequested(viewer_core::ImageId id, ThumbnailAction action);

private:
    const ThumbnailModel& m_model;
    std::array<QIcon, 3> m_icons; // Rename, Duplicate, Delete
};
