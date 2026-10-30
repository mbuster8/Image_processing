#include "display_text.h"
#include "entry_test_support.h"
#include "image_sidebar.h"
#include "palette_support.h"
#include "shades.h"
#include "thumbnail_model.h"
#include "thumbnail_panel.h"

#include "viewer_core/image_history.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QImage>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPalette>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QScrollBar>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using test_support::makeEntry;
using viewer_core::ImageId;

viewer_core::ReceivedImage makeEntryModifiedAt(ImageId id, std::string name,
                                               std::chrono::seconds modifiedAt) {
    auto entry = makeEntry(id, std::move(name));
    entry.origin.modifiedAt = std::chrono::system_clock::time_point(modifiedAt);
    return entry;
}

void showAtWidth(QWidget& widget, int width) {
    widget.resize(width, 600);
    widget.show();
    QApplication::processEvents();
}

QString captionOfRow(const ThumbnailPanel& panel, int row) {
    return panel.model()->index(row, 0).data(Qt::DisplayRole).toString();
}

QImage grabViewport(ThumbnailPanel& panel) { return panel.viewport()->grab().toImage(); }

// Pixels inside `rect` (viewport coordinates) whose colour is not `colour`. The image keeps the grab's device
// pixel ratio, and pixelColor counts device pixels.
int countPixelsOtherThan(const QImage& image, const QRect& rect, const QColor& colour) {
    const qreal ratio = image.devicePixelRatio();
    int count = 0;
    const QRect device(QPoint(qRound(rect.left() * ratio), qRound(rect.top() * ratio)), rect.size() * ratio);
    for (int y = device.top(); y <= device.bottom(); ++y) {
        for (int x = device.left(); x <= device.right(); ++x) {
            count += image.pixelColor(x, y).rgb() != colour.rgb() ? 1 : 0;
        }
    }
    return count;
}

TEST(ThumbnailPanel, NewestImageIsFirstAndSelected) {
    ThumbnailPanel panel;
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));

    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{2, 1}));
    EXPECT_EQ(panel.currentImageId(), std::optional<ImageId>(2));
    ASSERT_FALSE(selected.isEmpty());
    EXPECT_EQ(selected.last().at(0).value<quint64>(), 2u);
}

TEST(ThumbnailPanel, CaptionFollowsShownFields) {
    ThumbnailPanel panel;
    const auto entry = makeEntry(1, "name");
    panel.addImage(entry);
    const QString details = imageDetails(entry);
    ASSERT_TRUE(details.startsWith(QString::fromUtf8("4×3 · 1 ch · 8U · ")));

    EXPECT_EQ(panel.shownFields(), SidebarField::Thumbnail | SidebarField::Name | SidebarField::Details);
    EXPECT_EQ(captionOfRow(panel, 0), QStringLiteral("name\n") + details);

    panel.setShownFields(SidebarField::Name);
    EXPECT_EQ(captionOfRow(panel, 0), QStringLiteral("name"));

    panel.setShownFields(SidebarField::Details);
    EXPECT_EQ(captionOfRow(panel, 0), details);

    panel.setShownFields(SidebarField::Thumbnail);
    EXPECT_EQ(captionOfRow(panel, 0), QString());

    panel.setShownFields({});
    EXPECT_EQ(panel.shownFields(), SidebarFields(SidebarField::Thumbnail));
}

TEST(ThumbnailPanel, SelectingAnItemReportsItsId) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.setCurrentIndex(panel.model()->index(1, 0));

    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(selected.first().at(0).value<quint64>(), 1u);
}

// The window clears the choice while a live channel is shown, so a click on the image shown before it, or any
// other, shows that image again.
TEST(ThumbnailPanel, ClearChoiceLetsTheSameThumbnailBeChosenAgain) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.clearChoice();
    EXPECT_TRUE(selected.isEmpty());
    EXPECT_EQ(panel.currentImageId(), std::nullopt);
    EXPECT_TRUE(panel.selectionModel()->selectedIndexes().isEmpty());
    panel.addImage(makeEntry(2, "b"), false); // a change of the listing chooses nothing either
    EXPECT_EQ(panel.currentImageId(), std::nullopt);

    panel.setCurrentIndex(panel.model()->index(1, 0)); // "a", below the newer "b"

    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(selected.first().at(0).value<quint64>(), 1u);
}

TEST(ThumbnailPanel, RemovesEvictedImage) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));

    panel.removeImage(1);

    EXPECT_EQ(panel.listedIds(), std::vector<ImageId>{2});
}

TEST(ThumbnailPanel, EveryCellHasTheSameSize) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "portrait", 100, 10));
    panel.addImage(makeEntry(2, "landscape", 10, 100));
    panel.addImage(makeEntry(3, "small", 3, 4));
    showAtWidth(panel, 220);

    const QSize size = panel.cellRect(1).size();

    EXPECT_FALSE(size.isEmpty());
    EXPECT_EQ(panel.cellRect(2).size(), size);
    EXPECT_EQ(panel.cellRect(3).size(), size);
}

TEST(ThumbnailPanel, CentresPortraitThumbnail) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "portrait", 100, 10));
    panel.addImage(makeEntry(2, "landscape", 10, 100));
    showAtWidth(panel, 220);

    const QRect portrait = panel.thumbnailRect(1);
    const QRect landscape = panel.thumbnailRect(2); // as wide as the picture box, so its centre is the box's
    const QRect portraitCell = panel.cellRect(1);
    const QRect landscapeCell = panel.cellRect(2);

    ASSERT_FALSE(portrait.isEmpty());
    EXPECT_LT(portrait.width(), landscape.width());
    EXPECT_LE(std::abs((portrait.center().x() - portraitCell.left()) -
                       (landscape.center().x() - landscapeCell.left())),
              1);
    // The icon column keeps at least a 20 px icon's width beside the picture box.
    EXPECT_LE(landscape.right(), landscapeCell.right() - 20);
}

TEST(ThumbnailPanel, LongNameKeepsCellSize) {
    ThumbnailPanel panel;
    std::string longName;
    for (int i = 0; i < 300; ++i) {
        longName += "ž😀";
    }
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, longName));
    showAtWidth(panel, 220);

    EXPECT_EQ(panel.cellRect(2).size(), panel.cellRect(1).size());
}

TEST(ThumbnailPanel, NeverShowsAHorizontalScrollBar) {
    ThumbnailPanel panel;
    for (ImageId id = 1; id <= 5; ++id) {
        panel.addImage(makeEntry(id, std::string(400, 'w'), 10, 1000));
    }
    showAtWidth(panel, ImageSidebar::kMinimumWidth);

    EXPECT_EQ(panel.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_EQ(panel.horizontalScrollBar()->maximum(), 0);
}

TEST(ThumbnailPanel, CellsFollowThePanelWidth) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "four by three", 300, 400)); // fills the picture box exactly
    showAtWidth(panel, 150);
    const int narrowWidth = panel.cellRect(1).width();

    showAtWidth(panel, 300);
    const QRect cell = panel.cellRect(1);
    const QRect picture = panel.thumbnailRect(1);

    EXPECT_GT(cell.width(), narrowWidth);
    EXPECT_LE(std::abs(picture.width() * 3 - picture.height() * 4), 4);
}

// The selected cell is a neutral grey, slightly stronger while the list has focus (spec H2).
TEST(ThumbnailPanel, SelectionIsGreyAndStrongerWithFocus) {
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* other = new QLineEdit;
    auto* panel = new ThumbnailPanel;
    layout->addWidget(other);
    layout->addWidget(panel);
    panel->addImage(makeEntry(1, "a"));
    panel->addImage(makeEntry(2, "b"));
    showAtWidth(window, 220);
    window.activateWindow();
    other->setFocus();
    ASSERT_TRUE(panel->selectImage(1));
    QApplication::processEvents();
    const QPalette palette = QGuiApplication::palette();
    const auto insideFirst = [panel](const QImage& image) {
        return image.pixelColor((panel->cellRect(1).topLeft() + QPoint(2, 2)) * image.devicePixelRatio());
    };

    ASSERT_FALSE(panel->hasFocus());
    EXPECT_EQ(insideFirst(grabViewport(*panel)), qt_common::selectionGrey(palette, false));

    panel->setFocus();
    QApplication::processEvents();
    ASSERT_TRUE(panel->hasFocus());
    EXPECT_EQ(insideFirst(grabViewport(*panel)), qt_common::selectionGrey(palette, true));

    // And in a names-only list.
    other->setFocus();
    panel->setShownFields(SidebarField::Name);
    QApplication::processEvents();
    EXPECT_EQ(insideFirst(grabViewport(*panel)), qt_common::selectionGrey(palette, false));
}

// The cell under the pointer is a lighter grey than a selected one (spec H2).
TEST(ThumbnailPanel, HoveredCellIsALighterGrey) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    showAtWidth(panel, 220);
    const QPoint inside = panel.cellRect(1).topLeft() + QPoint(2, 2);

    QHoverEvent hover(QEvent::HoverMove, inside, panel.viewport()->mapToGlobal(inside), QPointF(-1, -1));
    QApplication::sendEvent(panel.viewport(), &hover);
    QApplication::processEvents();
    const QImage image = grabViewport(panel);

    EXPECT_EQ(image.pixelColor(inside * image.devicePixelRatio()),
              qt_common::hoverGrey(QGuiApplication::palette()));
}

// Captions and icons keep their normal colours on a selected cell: no accent anywhere (spec H2).
TEST(ThumbnailPanel, SelectedCellKeepsItsTextColours) {
    const test_support::PaletteGuard guard;
    QPalette accented = QGuiApplication::palette();
    accented.setColor(QPalette::HighlightedText, Qt::red);
    accented.setColor(QPalette::Highlight, Qt::blue);
    test_support::applyPalette(accented);
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    showAtWidth(panel, 220);

    const QImage image = grabViewport(panel);
    const QRect cell = panel.cellRect(1);
    EXPECT_EQ(countPixelsOtherThan(image, cell, Qt::red), cell.width() * cell.height());
    EXPECT_EQ(countPixelsOtherThan(image, cell, Qt::blue), cell.width() * cell.height());
}

// A 1-point line in the divider grey separates neighbouring cells, and none follows the last (spec H6).
TEST(ThumbnailPanel, LinesSeparateNeighbouringCells) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    panel.addImage(makeEntry(3, "c"), false);
    showAtWidth(panel, 220);
    const QImage image = grabViewport(panel);
    const QColor divider = qt_common::dividerGrey(QGuiApplication::palette());
    const auto bottomMiddle = [&](ImageId id) {
        const QRect cell = panel.cellRect(id);
        return image.pixelColor(QPoint(cell.center().x(), cell.bottom()) * image.devicePixelRatio());
    };

    EXPECT_EQ(bottomMiddle(3), divider);
    EXPECT_EQ(bottomMiddle(2), divider); // the selected cell too
    EXPECT_NE(bottomMiddle(1), divider);
}

TEST(ThumbnailPanel, InsertsAtHistoryIndexWithoutSelecting) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(3, "c"));

    panel.insertImage(makeEntry(2, "b"), 1, false);

    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{3, 2, 1}));
    EXPECT_EQ(panel.currentImageId(), std::optional<ImageId>(3));
}

TEST(ThumbnailPanel, UpdateImageRefreshesCaption) {
    ThumbnailPanel panel;
    auto entry = makeEntry(1, "old");
    panel.addImage(entry);

    entry.name = "new";
    panel.updateImage(entry);

    EXPECT_TRUE(captionOfRow(panel, 0).startsWith(QStringLiteral("new\n")));
}

TEST(ThumbnailPanel, SelectingAnUnknownIdReturnsFalse) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"), false);

    EXPECT_FALSE(panel.selectImage(99));
    EXPECT_EQ(panel.currentImageId(), std::nullopt);
}

TEST(ThumbnailPanel, ShowsIconsOnSelectedItem) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    showAtWidth(panel, 220);

    const QRect selectedIcon = panel.iconRect(2, ThumbnailAction::Delete);
    const QImage image = grabViewport(panel);

    ASSERT_TRUE(selectedIcon.isValid());
    EXPECT_TRUE(panel.cellRect(2).contains(selectedIcon));
    EXPECT_GT(selectedIcon.left(), panel.thumbnailRect(2).right());
    EXPECT_GT(countPixelsOtherThan(image, selectedIcon,
                                   qt_common::selectionGrey(QGuiApplication::palette(), false)),
              0);
    // Neither selected nor hovered: the icon's place is plain background.
    EXPECT_EQ(countPixelsOtherThan(image, panel.iconRect(1, ThumbnailAction::Delete),
                                   panel.palette().color(QPalette::Base)),
              0);
}

TEST(ThumbnailPanel, OnlyThreeActionsHaveIcons) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    showAtWidth(panel, 220);

    EXPECT_TRUE(panel.iconRect(1, ThumbnailAction::Rename).isValid());
    EXPECT_TRUE(panel.iconRect(1, ThumbnailAction::Duplicate).isValid());
    EXPECT_FALSE(panel.iconRect(1, ThumbnailAction::CopyImage).isValid());
    EXPECT_FALSE(panel.iconRect(1, ThumbnailAction::SaveAs).isValid());
    EXPECT_FALSE(panel.iconRect(99, ThumbnailAction::Delete).isValid());
}

TEST(ThumbnailPanel, ClickingAnIconRequestsItsAction) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    showAtWidth(panel, 220);
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);
    QSignalSpy requested(&panel, &ThumbnailPanel::actionRequested);

    QTest::mouseClick(panel.viewport(), Qt::LeftButton, {},
                      panel.iconRect(1, ThumbnailAction::Delete).center());

    ASSERT_EQ(requested.size(), 1);
    EXPECT_EQ(requested.first().at(0).value<quint64>(), 1u);
    EXPECT_EQ(requested.first().at(1).value<ThumbnailAction>(), ThumbnailAction::Delete);
    for (const auto& arguments : selected) {
        EXPECT_EQ(arguments.at(0).value<quint64>(), 1u);
    }
}

TEST(ThumbnailPanel, ContextMenuOffersAllActions) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));

    const auto menu = panel.createContextMenu(1);
    QStringList texts;
    for (const QAction* action : menu->actions()) {
        if (!action->isSeparator()) {
            texts << action->text();
        }
    }

    EXPECT_EQ(texts, (QStringList{QString::fromUtf8("Rename…"), QStringLiteral("Duplicate"),
                                  QStringLiteral("Copy Image"), QString::fromUtf8("Save As…"),
                                  QStringLiteral("Delete")}));
}

TEST(ThumbnailPanel, ContextMenuActionRequestsItForThatImage) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    QSignalSpy requested(&panel, &ThumbnailPanel::actionRequested);

    const auto menu = panel.createContextMenu(1);
    for (QAction* action : menu->actions()) {
        if (action->text() == QStringLiteral("Copy Image")) {
            action->trigger();
        }
    }

    ASSERT_EQ(requested.size(), 1);
    EXPECT_EQ(requested.first().at(0).value<quint64>(), 1u);
    EXPECT_EQ(requested.first().at(1).value<ThumbnailAction>(), ThumbnailAction::CopyImage);
}

TEST(ThumbnailPanel, DeleteKeysRequestDelete) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    showAtWidth(panel, 220);
    panel.setFocus();
    QSignalSpy requested(&panel, &ThumbnailPanel::actionRequested);

    QTest::keyClick(&panel, Qt::Key_Delete);
    QTest::keyClick(&panel, Qt::Key_Backspace);

    ASSERT_EQ(requested.size(), 2);
    for (const auto& arguments : requested) {
        EXPECT_EQ(arguments.at(0).value<quint64>(), 2u);
        EXPECT_EQ(arguments.at(1).value<ThumbnailAction>(), ThumbnailAction::Delete);
    }
}

TEST(ThumbnailPanel, SearchListsMatchingNamesOnly) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "Canny edges"));
    panel.addImage(makeEntry(2, "Sobel X"));
    panel.addImage(makeEntry(3, "canny copy"));
    panel.addImage(makeEntry(4, "Žltá maska"));

    panel.setSearchText(QStringLiteral("CANNY"));
    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{3, 1}));
    EXPECT_EQ(panel.listedCount(), 2);
    EXPECT_EQ(panel.imageCount(), 4);

    panel.setSearchText(QString::fromUtf8("ž"));
    EXPECT_EQ(panel.listedIds(), std::vector<ImageId>{4});

    panel.setSearchText(QString());
    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{4, 3, 2, 1}));
}

TEST(ThumbnailPanel, SearchNeverChangesTheSelection) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    panel.addImage(makeEntry(3, "c"));
    ASSERT_TRUE(panel.selectImage(2));
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.setSearchText(QStringLiteral("a"));
    EXPECT_TRUE(selected.isEmpty());
    EXPECT_EQ(panel.currentImageId(), std::nullopt);

    panel.setSearchText(QString());
    EXPECT_TRUE(selected.isEmpty());
    EXPECT_EQ(panel.currentImageId(), std::optional<ImageId>(2));
    EXPECT_TRUE(panel.selectionModel()->isSelected(panel.currentIndex()));
}

// The window chooses the next image (Task 12); the list must not pick one by itself.
TEST(ThumbnailPanel, RemovingTheSelectedRowEmitsNothing) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    panel.addImage(makeEntry(3, "c"));
    ASSERT_TRUE(panel.selectImage(2));
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.removeImage(2);

    EXPECT_TRUE(selected.isEmpty());
    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{3, 1}));
    EXPECT_EQ(panel.currentImageId(), std::nullopt);
}

TEST(ThumbnailPanel, SortsByNameNumbersByValue) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "stage 10"));
    panel.addImage(makeEntry(2, "stage 2"));
    panel.addImage(makeEntry(3, "Stage 1"));

    panel.setSort(SidebarSort::Name, Qt::AscendingOrder);

    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{3, 2, 1}));
}

// Qt's collator compares plain strings in the C locale, which a Linux system with LANG=C.UTF-8 (such as a CI
// runner) reports, so numbers would sort as text there.
TEST(ThumbnailPanel, SortsNumbersByValueInTheCLocale) {
    const QLocale previous;
    QLocale::setDefault(QLocale::c());
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "stage 10"));
    panel.addImage(makeEntry(2, "stage 2"));
    panel.addImage(makeEntry(3, "Stage 1"));

    panel.setSort(SidebarSort::Name, Qt::AscendingOrder);
    QLocale::setDefault(previous);

    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{3, 2, 1}));
}

TEST(ThumbnailPanel, SortsBySizeLargestFirst) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "ten", 10, 10));
    panel.addImage(makeEntry(2, "hundred", 100, 100));
    panel.addImage(makeEntry(3, "fifty", 50, 50));

    panel.setSort(SidebarSort::Size, ThumbnailPanel::naturalOrder(SidebarSort::Size));

    EXPECT_EQ(ThumbnailPanel::naturalOrder(SidebarSort::Size), Qt::DescendingOrder);
    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{2, 3, 1}));
}

TEST(ThumbnailPanel, SortsByModifiedTime) {
    using std::chrono::seconds;
    ThumbnailPanel panel;
    panel.addImage(makeEntryModifiedAt(1, "a", seconds(1)));
    panel.addImage(makeEntryModifiedAt(2, "b", seconds(2)));
    panel.addImage(makeEntryModifiedAt(3, "c", seconds(3)));
    panel.setSort(SidebarSort::Modified, ThumbnailPanel::naturalOrder(SidebarSort::Modified));
    ASSERT_EQ(panel.listedIds(), (std::vector<ImageId>{3, 2, 1}));

    panel.updateImage(makeEntryModifiedAt(1, "a", seconds(4)));

    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{1, 3, 2}));
}

TEST(ThumbnailPanel, ReceivedOrderIsHistoryOrder) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "b"));
    panel.addImage(makeEntry(3, "a"));
    panel.setSort(SidebarSort::Name, Qt::AscendingOrder);
    panel.insertImage(makeEntry(2, "c"), 1, false);

    panel.setSort(SidebarSort::Received, Qt::DescendingOrder);

    EXPECT_EQ(ThumbnailPanel::naturalOrder(SidebarSort::Received), Qt::DescendingOrder);
    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{3, 2, 1}));
}

TEST(ThumbnailPanel, TiesKeepHistoryOrderInBothDirections) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "x"));
    panel.addImage(makeEntry(2, "x"));

    panel.setSort(SidebarSort::Name, Qt::AscendingOrder);
    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{2, 1}));

    panel.setSort(SidebarSort::Name, Qt::DescendingOrder);
    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{2, 1}));
}

TEST(ThumbnailPanel, EveryLayoutKeepsEqualCells) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "portrait", 100, 10));
    panel.addImage(makeEntry(2, "landscape", 10, 100));
    panel.addImage(makeEntry(3, std::string(400, 'w')));
    showAtWidth(panel, 220);

    for (int bits = 1; bits <= 7; ++bits) {
        SCOPED_TRACE(bits);
        panel.setShownFields(SidebarFields::fromInt(bits));
        QApplication::processEvents();

        EXPECT_FALSE(panel.cellRect(1).isEmpty());
        EXPECT_EQ(panel.cellRect(1).size(), panel.cellRect(2).size());
        EXPECT_EQ(panel.cellRect(3).size(), panel.cellRect(2).size());
    }
}

TEST(ThumbnailPanel, NamesOnlyRowsAreCompact) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    showAtWidth(panel, 220);
    const int fullHeight = panel.cellRect(1).height();

    panel.setShownFields(SidebarField::Name);
    QApplication::processEvents();
    const QRect cell = panel.cellRect(1);
    const QRect rename = panel.iconRect(1, ThumbnailAction::Rename);
    const QRect duplicate = panel.iconRect(1, ThumbnailAction::Duplicate);
    const QRect remove = panel.iconRect(1, ThumbnailAction::Delete);

    EXPECT_LT(cell.height() * 4, fullHeight);
    EXPECT_TRUE(panel.thumbnailRect(1).isEmpty());
    EXPECT_EQ(rename.center().y(), duplicate.center().y());
    EXPECT_EQ(duplicate.center().y(), remove.center().y());
    EXPECT_LT(rename.right(), duplicate.left());
    EXPECT_LT(duplicate.right(), remove.left());
    EXPECT_TRUE(cell.contains(remove));
    EXPECT_GT(rename.left(), cell.center().x()); // the text has the rest of the row
}

TEST(ThumbnailPanel, IconsWorkInCompactRows) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    showAtWidth(panel, 220);
    panel.setShownFields(SidebarField::Name);
    QApplication::processEvents();
    QSignalSpy requested(&panel, &ThumbnailPanel::actionRequested);

    QTest::mouseClick(panel.viewport(), Qt::LeftButton, {},
                      panel.iconRect(1, ThumbnailAction::Delete).center());

    ASSERT_EQ(requested.size(), 1);
    EXPECT_EQ(requested.first().at(0).value<quint64>(), 1u);
    EXPECT_EQ(requested.first().at(1).value<ThumbnailAction>(), ThumbnailAction::Delete);
}

TEST(ThumbnailPanel, TooltipShowsHiddenText) {
    ThumbnailPanel panel;
    auto entry = makeEntry(1, "name");
    entry.origin.arrivedAt = std::chrono::system_clock::time_point(std::chrono::hours(1));
    entry.origin.modifiedAt = entry.origin.arrivedAt;
    panel.addImage(entry);

    const QString unchanged = panel.model()->index(0, 0).data(Qt::ToolTipRole).toString();
    EXPECT_TRUE(unchanged.contains(QStringLiteral("name")));
    EXPECT_TRUE(unchanged.contains(imageDetails(entry)));
    EXPECT_TRUE(unchanged.contains(QStringLiteral("Received ")));
    EXPECT_FALSE(unchanged.contains(QStringLiteral("Modified ")));

    entry.origin.modifiedAt += std::chrono::minutes(5);
    panel.updateImage(entry);
    EXPECT_TRUE(
        panel.model()->index(0, 0).data(Qt::ToolTipRole).toString().contains(QStringLiteral("Modified ")));
}

TEST(ThumbnailPanel, RenameResortsAndKeepsSelection) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    panel.addImage(makeEntry(3, "c"));
    panel.setSort(SidebarSort::Name, Qt::AscendingOrder);
    ASSERT_TRUE(panel.selectImage(2));
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.updateImage(makeEntry(2, "z"));

    EXPECT_EQ(panel.listedIds(), (std::vector<ImageId>{1, 3, 2}));
    EXPECT_EQ(panel.currentImageId(), std::optional<ImageId>(2));
    EXPECT_TRUE(panel.selectionModel()->isSelected(panel.currentIndex()));
    EXPECT_TRUE(selected.isEmpty());
}

} // namespace
