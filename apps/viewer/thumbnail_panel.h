#pragma once

#include "thumbnail_delegate.h"
#include "thumbnail_model.h"

#include "viewer_core/image_history.h"

#include <QListView>
#include <QRect>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QMenu;

// The history down the side of the viewer: each image as its thumbnail, name and details, or the ones of
// those that are shown, in equal cells that follow the panel's width, searched and sorted (newest first by
// default). Rows in this API are history indexes; everything else is keyed by id, because the order shown
// need not be the history's.
//
// The panel remembers the image last chosen, by the user or by selectImage. Searching, sorting and removing
// rows never choose another: the remembered image stays selected while listed, and nothing is selected while
// the search hides it. imageSelected is emitted only when the remembered image changes.
class ThumbnailPanel : public QListView {
    Q_OBJECT

public:
    explicit ThumbnailPanel(QWidget* parent = nullptr);

    // Adds the image at history index 0.
    void addImage(const viewer_core::ReceivedImage& image, bool select = true);
    void insertImage(const viewer_core::ReceivedImage& image, int historyIndex, bool select);
    void removeImage(viewer_core::ImageId id);
    // Refreshes the caption and the sort keys from the entry, after a rename.
    void updateImage(const viewer_core::ReceivedImage& image);

    // Makes the image current and selected. False, leaving nothing selected, when the id is unknown or not
    // listed; an image the search hides is still remembered, and selected again once listed.
    bool selectImage(viewer_core::ImageId id);
    std::optional<viewer_core::ImageId> currentImageId() const;
    // Quietly selects nothing and forgets the remembered image, so choosing any thumbnail afterwards, the one
    // chosen before included, emits imageSelected. For while the window shows something else.
    void clearChoice();
    bool isListed(viewer_core::ImageId id) const;
    std::vector<viewer_core::ImageId> listedIds() const; // top to bottom, as shown
    int listedCount() const;
    int imageCount() const; // listed or not

    void setSearchText(const QString& text); // lists names containing it, ignoring case; "" lists all
    void setSort(SidebarSort key, Qt::SortOrder order);
    // Descending (newest or largest first) for Received, Modified and Size; Ascending (A to Z) for Name.
    static Qt::SortOrder naturalOrder(SidebarSort key);

    void setShownFields(SidebarFields fields); // an empty set is ignored
    SidebarFields shownFields() const;         // all three by default

    // In viewport coordinates; empty when the id is not listed. thumbnailRect is the picture as drawn.
    QRect cellRect(viewer_core::ImageId id) const;
    QRect thumbnailRect(viewer_core::ImageId id) const;
    QRect iconRect(viewer_core::ImageId id,
                   ThumbnailAction action) const; // also empty for an action without one

    // The right-click menu for the image: every ThumbnailAction, each requesting itself for that image.
    std::unique_ptr<QMenu> createContextMenu(viewer_core::ImageId id);

signals:
    void imageSelected(viewer_core::ImageId id);
    // From the cell's icons, the right-click menu, and Delete or Backspace while the panel has focus.
    void actionRequested(viewer_core::ImageId id, ThumbnailAction action);
    void listedChanged(); // after the search, the sort or the images change

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private:
    QModelIndex viewIndexOf(viewer_core::ImageId id) const;
    // Runs a change to the rows or the listing, ignoring the current-index moves the view makes by itself,
    // then puts the selection back on the remembered image.
    void changeListing(const std::function<void()>& change);
    void restoreSelection();

    ThumbnailModel* m_model;
    ThumbnailFilterModel* m_filter;
    std::optional<viewer_core::ImageId> m_selectedId; // the remembered image
    int m_quiet = 0;                                  // > 0 while the panel itself moves the current index
};
