#include "control_checks.h"
#include "entry_test_support.h"
#include "image_sidebar.h"
#include "thumbnail_model.h"
#include "thumbnail_panel.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QToolButton>

#include <gtest/gtest.h>

#include <vector>

namespace {

using test_support::makeEntry;
using viewer_core::ImageId;

std::vector<QAction*> actionsWithoutSeparators(const QMenu& menu) {
    std::vector<QAction*> actions;
    for (QAction* action : menu.actions()) {
        if (!action->isSeparator()) {
            actions.push_back(action);
        }
    }
    return actions;
}

QMenu* submenuTitled(const QMenu& menu, const QString& title) {
    for (QMenu* submenu : menu.findChildren<QMenu*>()) {
        if (submenu->title() == title) {
            return submenu;
        }
    }
    return nullptr;
}

// The search row is large enough to see (spec H1).
TEST(ImageSidebar, SearchRowIsLargeEnoughToSee) {
    ImageSidebar sidebar;
    sidebar.show();

    EXPECT_TRUE(test_support::isTallEnough(sidebar.searchField()));
    EXPECT_TRUE(
        test_support::isLargeFramedButton(sidebar.findChild<QToolButton*>(QStringLiteral("sidebarOptions"))));
}

TEST(ImageSidebar, CountShowsOnlyWhileSearching) {
    ImageSidebar sidebar;
    sidebar.thumbnails()->addImage(makeEntry(1, "a1"));
    sidebar.thumbnails()->addImage(makeEntry(2, "b"));
    sidebar.thumbnails()->addImage(makeEntry(3, "c"));
    EXPECT_TRUE(sidebar.countLabel()->isHidden());

    QTest::keyClicks(sidebar.searchField(), QStringLiteral("a"));

    EXPECT_FALSE(sidebar.countLabel()->isHidden());
    EXPECT_EQ(sidebar.countLabel()->text(), QStringLiteral("1 of 3"));
}

TEST(ImageSidebar, EscapeClearsThenFocusesList) {
    ImageSidebar sidebar;
    sidebar.thumbnails()->addImage(makeEntry(1, "a"));
    sidebar.show();
    sidebar.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&sidebar));
    QLineEdit* field = sidebar.searchField();
    field->setFocus();
    QTest::keyClicks(field, QStringLiteral("x"));

    QTest::keyClick(field, Qt::Key_Escape);
    EXPECT_TRUE(field->text().isEmpty());
    EXPECT_TRUE(field->hasFocus());

    QTest::keyClick(field, Qt::Key_Escape);
    EXPECT_TRUE(sidebar.thumbnails()->hasFocus());
}

// A list that gains focus (by Tab, or when the window is activated again) with nothing selected would make
// its first row current, and so show an image while the search hides the one in the view.
TEST(ImageSidebar, FocusingTheListKeepsAHiddenSelection) {
    ImageSidebar sidebar;
    ThumbnailPanel* panel = sidebar.thumbnails();
    sidebar.searchField()->setText(QStringLiteral("a"));
    panel->addImage(makeEntry(1, "a"), false);
    panel->addImage(makeEntry(2, "b"), false);
    ASSERT_FALSE(panel->selectImage(2)); // the image in the view, hidden by the search
    sidebar.show();
    sidebar.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&sidebar));
    QSignalSpy selected(panel, &ThumbnailPanel::imageSelected);

    panel->setFocus(Qt::TabFocusReason);
    ASSERT_TRUE(panel->hasFocus());

    EXPECT_TRUE(selected.isEmpty());
    EXPECT_EQ(panel->currentImageId(), std::nullopt);
}

TEST(ImageSidebar, EnterSelectsFirstListed) {
    ImageSidebar sidebar;
    ThumbnailPanel* panel = sidebar.thumbnails();
    panel->addImage(makeEntry(1, "a1"), false);
    panel->addImage(makeEntry(2, "b"), false);
    panel->addImage(makeEntry(3, "a2"), false);
    QSignalSpy selected(panel, &ThumbnailPanel::imageSelected);
    QTest::keyClicks(sidebar.searchField(), QStringLiteral("a"));

    QTest::keyClick(sidebar.searchField(), Qt::Key_Return);

    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(selected.first().at(0).value<quint64>(), panel->listedIds().at(0));
    EXPECT_EQ(panel->listedIds().at(0), 3u);
}

TEST(ImageSidebar, ChoosingAKeySetsItsNaturalOrder) {
    ImageSidebar sidebar;

    sidebar.sortAction(SidebarSort::Name)->trigger();
    EXPECT_TRUE(sidebar.sortAction(SidebarSort::Name)->isChecked());
    EXPECT_TRUE(sidebar.orderAction(Qt::AscendingOrder)->isChecked());

    sidebar.sortAction(SidebarSort::Size)->trigger();
    EXPECT_TRUE(sidebar.sortAction(SidebarSort::Size)->isChecked());
    EXPECT_TRUE(sidebar.orderAction(Qt::DescendingOrder)->isChecked());
}

TEST(ImageSidebar, SortIsRemembered) {
    {
        ImageSidebar first;
        first.sortAction(SidebarSort::Name)->trigger();
        first.orderAction(Qt::DescendingOrder)->trigger();
    }

    ImageSidebar second;
    second.thumbnails()->addImage(makeEntry(1, "b"));
    second.thumbnails()->addImage(makeEntry(2, "c"));
    second.thumbnails()->addImage(makeEntry(3, "a"));

    EXPECT_TRUE(second.sortAction(SidebarSort::Name)->isChecked());
    EXPECT_TRUE(second.orderAction(Qt::DescendingOrder)->isChecked());
    EXPECT_EQ(second.thumbnails()->listedIds(), (std::vector<ImageId>{2, 1, 3}));
}

TEST(ImageSidebar, ViewMenuSharesTheActions) {
    ImageSidebar sidebar;
    QMenu viewMenu;

    sidebar.populateViewMenu(&viewMenu);

    QMenu* viewSort = submenuTitled(viewMenu, QStringLiteral("Sort By"));
    QMenu* optionsSort = submenuTitled(*sidebar.optionsMenu(), QStringLiteral("Sort By"));
    ASSERT_NE(viewSort, nullptr);
    ASSERT_NE(optionsSort, nullptr);
    EXPECT_NE(viewSort, optionsSort);
    EXPECT_EQ(actionsWithoutSeparators(*viewSort).size(), 6u);
    EXPECT_EQ(actionsWithoutSeparators(*viewSort), actionsWithoutSeparators(*optionsSort));
    EXPECT_EQ(actionsWithoutSeparators(*viewSort).front(), sidebar.sortAction(SidebarSort::Received));

    // Show ▸ sits above Sort By ▸ in both.
    QMenu* viewShow = submenuTitled(viewMenu, QStringLiteral("Show in Sidebar"));
    QMenu* optionsShow = submenuTitled(*sidebar.optionsMenu(), QStringLiteral("Show"));
    ASSERT_NE(viewShow, nullptr);
    ASSERT_NE(optionsShow, nullptr);
    EXPECT_EQ(actionsWithoutSeparators(*viewShow), actionsWithoutSeparators(*optionsShow));
    EXPECT_EQ(actionsWithoutSeparators(*viewShow),
              (std::vector<QAction*>{sidebar.showAction(SidebarField::Thumbnail),
                                     sidebar.showAction(SidebarField::Name),
                                     sidebar.showAction(SidebarField::Details)}));
    EXPECT_LT(viewMenu.actions().indexOf(viewShow->menuAction()),
              viewMenu.actions().indexOf(viewSort->menuAction()));
    EXPECT_LT(sidebar.optionsMenu()->actions().indexOf(optionsShow->menuAction()),
              sidebar.optionsMenu()->actions().indexOf(optionsSort->menuAction()));
}

TEST(ImageSidebar, LastShownFieldCannotBeUnticked) {
    ImageSidebar sidebar;
    QAction* name = sidebar.showAction(SidebarField::Name);

    sidebar.showAction(SidebarField::Thumbnail)->trigger();
    sidebar.showAction(SidebarField::Details)->trigger();
    EXPECT_TRUE(name->isChecked());
    EXPECT_FALSE(name->isEnabled());
    EXPECT_EQ(sidebar.thumbnails()->shownFields(), SidebarFields(SidebarField::Name));

    sidebar.showAction(SidebarField::Details)->trigger();
    EXPECT_TRUE(name->isEnabled());
}

TEST(ImageSidebar, ShownFieldsAreRemembered) {
    {
        ImageSidebar first;
        first.showAction(SidebarField::Thumbnail)->trigger();
        first.showAction(SidebarField::Details)->trigger();
    }

    ImageSidebar second;

    EXPECT_FALSE(second.showAction(SidebarField::Thumbnail)->isChecked());
    EXPECT_TRUE(second.showAction(SidebarField::Name)->isChecked());
    EXPECT_FALSE(second.showAction(SidebarField::Details)->isChecked());
    EXPECT_EQ(second.thumbnails()->shownFields(), SidebarFields(SidebarField::Name));
}

TEST(ImageSidebar, ListSpansTheFullWidth) {
    ImageSidebar sidebar;
    sidebar.resize(ImageSidebar::kDefaultWidth, 600);
    sidebar.show();
    QApplication::processEvents();

    EXPECT_EQ(sidebar.thumbnails()->width(), sidebar.width());
}

} // namespace
