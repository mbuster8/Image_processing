#pragma once

#include "thumbnail_model.h"

#include <QWidget>

#include <array>

class QAction;
class QActionGroup;
class QLabel;
class QLineEdit;
class QMenu;
class QToolButton;
class LiveChannelPanel;
class ThumbnailPanel;

// The splitter's first widget: the Live group while live channels exist, one row with the search field and
// the options button, the "2 of 10" count while searching, and the thumbnails below at the sidebar's full
// width. What items show and the sort are remembered; the search is not, so every start lists every image.
class ImageSidebar : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMinimumWidth = 120;
    static constexpr int kMaximumWidth = 400;
    static constexpr int kDefaultWidth = 220;

    explicit ImageSidebar(QWidget* parent = nullptr);

    LiveChannelPanel* liveChannels() const;
    ThumbnailPanel* thumbnails() const;
    QLineEdit* searchField() const;
    QLabel* countLabel() const;
    QMenu* optionsMenu() const; // Show ▸ and Sort By ▸

    QAction*
    showAction(SidebarField field) const; // "Thumbnail", "Name", "Details"; the last one ticked is disabled
    QAction* sortAction(SidebarSort key) const;      // "Time Received", "Time Modified", "Name", "Size"
    QAction* orderAction(Qt::SortOrder order) const; // "Ascending", "Descending"

    void focusSearch(); // focuses the field and selects its text

    // Adds Show in Sidebar ▸ and Sort By ▸ to viewMenu as new QMenus holding the same QActions as the options
    // menu. Never the same QMenu twice: a native macOS menu has one parent.
    void populateViewMenu(QMenu* viewMenu);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void addShowActions(QMenu* menu) const;
    void addSortActions(QMenu* menu) const;
    void loadShownFields();
    void applyShownFields(); // from the checked actions, and remembered
    void loadSort();
    void applySort(); // from the checked actions, and remembered
    void updateCount();

    LiveChannelPanel* m_live;
    ThumbnailPanel* m_thumbnails;
    QLineEdit* m_search;
    QToolButton* m_optionsButton;
    QLabel* m_count;
    QMenu* m_optionsMenu;
    std::array<QAction*, 3> m_showActions{};  // Thumbnail, Name, Details
    std::array<QAction*, 4> m_sortActions{};  // indexed by SidebarSort
    std::array<QAction*, 2> m_orderActions{}; // indexed by Qt::SortOrder
};
