#include "image_sidebar.h"
#include "live_channel_panel.h"
#include "shades.h"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QToolButton>
#include <QWidget>

#include <gtest/gtest.h>

#include <optional>

namespace {

QImage frame() {
    QImage image(64, 48, QImage::Format_RGB32);
    image.fill(Qt::darkGreen);
    return image;
}

// The panel inside a shown window, as in the sidebar.
struct ShownPanel {
    ShownPanel() : panel(new LiveChannelPanel(&window)) {
        window.resize(220, 600);
        window.show();
    }

    QWidget window;
    LiveChannelPanel* panel;
};

QWidget* rowOf(const LiveChannelPanel& panel, const QString& name) {
    return panel.findChild<QWidget*>(QStringLiteral("liveRow_") + name);
}

QString textIn(const QWidget* row, const char* objectName) {
    const auto* label = row->findChild<QLabel*>(QString::fromLatin1(objectName));
    return label != nullptr ? label->text() : QStringLiteral("<none>");
}

QToolButton* closeButtonOf(const QWidget* row) {
    return row->findChild<QToolButton*>(QStringLiteral("liveClose"));
}

QColor dotColourOf(const QWidget* row) {
    auto* dot = row->findChild<QWidget*>(QStringLiteral("liveDot"));
    const QImage image = dot->grab().toImage();
    return image.pixelColor(image.width() / 2, image.height() / 2);
}

TEST(LiveChannelPanel, HiddenUntilAChannelStarts) {
    ShownPanel shown;
    EXPECT_TRUE(shown.panel->isHidden());

    shown.panel->addChannel(QStringLiteral("camera"));

    EXPECT_TRUE(shown.panel->isVisible());
    EXPECT_EQ(shown.panel->channelNames(), QStringList{QStringLiteral("camera")});
}

TEST(LiveChannelPanel, RowsKeepStartOrder) {
    ShownPanel shown;
    shown.panel->addChannel(QStringLiteral("edges"));
    shown.panel->addChannel(QStringLiteral("camera"));
    shown.panel->addChannel(QStringLiteral("mask"));
    shown.panel->addChannel(QStringLiteral("camera")); // already there
    shown.panel->updateChannel(QStringLiteral("mask"), frame(), 30, LiveRowState::Active);
    QApplication::processEvents();

    EXPECT_EQ(shown.panel->channelNames(),
              (QStringList{QStringLiteral("edges"), QStringLiteral("camera"), QStringLiteral("mask")}));
    EXPECT_LT(rowOf(*shown.panel, QStringLiteral("edges"))->y(),
              rowOf(*shown.panel, QStringLiteral("camera"))->y());
    EXPECT_LT(rowOf(*shown.panel, QStringLiteral("camera"))->y(),
              rowOf(*shown.panel, QStringLiteral("mask"))->y());
}

TEST(LiveChannelPanel, ShowsRateAndState) {
    ShownPanel shown;
    shown.panel->addChannel(QStringLiteral("camera"));
    const QWidget* row = rowOf(*shown.panel, QStringLiteral("camera"));
    ASSERT_NE(row, nullptr);

    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 30, LiveRowState::Active);
    EXPECT_EQ(textIn(row, "liveRate"), QStringLiteral("30 fps"));
    EXPECT_EQ(textIn(row, "liveState"), QStringLiteral("live"));
    EXPECT_EQ(dotColourOf(row), qt_common::accentColour(QGuiApplication::palette()));
    const auto* thumbnail = row->findChild<QLabel*>(QStringLiteral("liveThumbnail"));
    ASSERT_NE(thumbnail, nullptr);
    EXPECT_FALSE(thumbnail->pixmap().isNull());
    EXPECT_LE(thumbnail->height(), LiveChannelPanel::kRowHeight);

    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 0, LiveRowState::Idle);
    EXPECT_EQ(textIn(row, "liveRate"), QStringLiteral("0 fps"));
    EXPECT_EQ(textIn(row, "liveState"), QStringLiteral("idle"));
    EXPECT_NE(dotColourOf(row), qt_common::accentColour(QGuiApplication::palette()));

    // Frozen says so; the dot still shows whether frames arrive.
    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 25, LiveRowState::Frozen);
    EXPECT_EQ(textIn(row, "liveState"), QStringLiteral("frozen"));
    EXPECT_EQ(dotColourOf(row), qt_common::accentColour(QGuiApplication::palette()));
    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 0, LiveRowState::Frozen);
    EXPECT_NE(dotColourOf(row), qt_common::accentColour(QGuiApplication::palette()));
}

TEST(LiveChannelPanel, CloseOnlyOnIdleRows) {
    ShownPanel shown;
    shown.panel->addChannel(QStringLiteral("camera"));
    const QWidget* row = rowOf(*shown.panel, QStringLiteral("camera"));
    EXPECT_TRUE(closeButtonOf(row)->isHidden());

    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 30, LiveRowState::Active);
    EXPECT_TRUE(closeButtonOf(row)->isHidden());
    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 30, LiveRowState::Frozen);
    EXPECT_TRUE(closeButtonOf(row)->isHidden());
    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 0, LiveRowState::Idle);
    EXPECT_FALSE(closeButtonOf(row)->isHidden());
    EXPECT_FALSE(closeButtonOf(row)->toolTip().isEmpty());
}

TEST(LiveChannelPanel, ClickSelectsAndEmits) {
    ShownPanel shown;
    shown.panel->addChannel(QStringLiteral("camera"));
    shown.panel->addChannel(QStringLiteral("edges"));
    QApplication::processEvents();
    QSignalSpy selected(shown.panel, &LiveChannelPanel::channelSelected);

    QTest::mouseClick(rowOf(*shown.panel, QStringLiteral("edges")), Qt::LeftButton);

    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(selected.first().at(0).toString(), QStringLiteral("edges"));
    EXPECT_EQ(shown.panel->selectedChannel(), QStringLiteral("edges"));
}

// Choosing a channel leaves the keyboard where it was, so the arrow keys still step through the thumbnails.
TEST(LiveChannelPanel, ClickLeavesTheFocusWhereItWas) {
    QWidget window;
    auto* field = new QLineEdit(&window);
    auto* panel = new LiveChannelPanel(&window);
    panel->move(0, 40);
    window.resize(220, 300);
    window.show();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
    panel->addChannel(QStringLiteral("camera"));
    QApplication::processEvents();
    field->setFocus();

    QTest::mouseClick(rowOf(*panel, QStringLiteral("camera")), Qt::LeftButton);

    EXPECT_EQ(panel->selectedChannel(), QStringLiteral("camera"));
    EXPECT_TRUE(field->hasFocus());
}

TEST(LiveChannelPanel, SetSelectedIsQuiet) {
    ShownPanel shown;
    shown.panel->addChannel(QStringLiteral("camera"));
    QSignalSpy selected(shown.panel, &LiveChannelPanel::channelSelected);

    shown.panel->setSelected(QStringLiteral("camera"));
    EXPECT_EQ(shown.panel->selectedChannel(), QStringLiteral("camera"));
    shown.panel->setSelected(std::nullopt);
    EXPECT_EQ(shown.panel->selectedChannel(), std::nullopt);
    shown.panel->setSelected(QStringLiteral("nobody"));
    EXPECT_EQ(shown.panel->selectedChannel(), std::nullopt);

    EXPECT_TRUE(selected.isEmpty());
}

TEST(LiveChannelPanel, CloseButtonEmits) {
    ShownPanel shown;
    shown.panel->addChannel(QStringLiteral("camera"));
    shown.panel->updateChannel(QStringLiteral("camera"), frame(), 0, LiveRowState::Idle);
    QSignalSpy closeRequested(shown.panel, &LiveChannelPanel::closeRequested);
    QSignalSpy selected(shown.panel, &LiveChannelPanel::channelSelected);

    closeButtonOf(rowOf(*shown.panel, QStringLiteral("camera")))->click();

    ASSERT_EQ(closeRequested.size(), 1);
    EXPECT_EQ(closeRequested.first().at(0).toString(), QStringLiteral("camera"));
    EXPECT_TRUE(selected.isEmpty());
    EXPECT_EQ(shown.panel->channelNames(), QStringList{QStringLiteral("camera")}); // the window decides
}

TEST(LiveChannelPanel, HidesWhenTheLastRowGoes) {
    ShownPanel shown;
    shown.panel->addChannel(QStringLiteral("camera"));
    shown.panel->addChannel(QStringLiteral("edges"));
    shown.panel->setSelected(QStringLiteral("camera"));

    shown.panel->removeChannel(QStringLiteral("camera"));
    EXPECT_TRUE(shown.panel->isVisible());
    EXPECT_EQ(shown.panel->selectedChannel(), std::nullopt);
    EXPECT_EQ(rowOf(*shown.panel, QStringLiteral("camera")), nullptr);

    shown.panel->removeChannel(QStringLiteral("edges"));
    EXPECT_TRUE(shown.panel->isHidden());
    EXPECT_TRUE(shown.panel->channelNames().isEmpty());
}

TEST(LiveChannelPanel, ScrollsBeyondThreeRows) {
    ShownPanel shown;
    auto* scroll = shown.panel->findChild<QScrollArea*>();
    ASSERT_NE(scroll, nullptr);

    shown.panel->addChannel(QStringLiteral("a"));
    shown.panel->addChannel(QStringLiteral("b"));
    QApplication::processEvents();
    EXPECT_EQ(scroll->viewport()->height(), 2 * LiveChannelPanel::kRowHeight);
    EXPECT_EQ(scroll->verticalScrollBar()->maximum(), 0);

    shown.panel->addChannel(QStringLiteral("c"));
    shown.panel->addChannel(QStringLiteral("d"));
    // The scroll range follows the rows' layout through posted events, which the running viewer's loop
    // delivers.
    EXPECT_TRUE(QTest::qWaitFor([scroll] { return scroll->verticalScrollBar()->maximum() > 0; }));
    EXPECT_EQ(scroll->viewport()->height(), LiveChannelPanel::kVisibleRows * LiveChannelPanel::kRowHeight);
    EXPECT_EQ(scroll->verticalScrollBar()->maximum(), LiveChannelPanel::kRowHeight);
}

TEST(LiveChannelPanel, SidebarShowsItAboveTheSearch) {
    ImageSidebar sidebar;
    sidebar.resize(ImageSidebar::kDefaultWidth, 600);
    sidebar.show();
    LiveChannelPanel* panel = sidebar.liveChannels();
    ASSERT_NE(panel, nullptr);
    EXPECT_TRUE(panel->isHidden());

    panel->addChannel(QStringLiteral("camera"));
    QApplication::processEvents();

    EXPECT_TRUE(panel->isVisible());
    EXPECT_EQ(panel->width(), sidebar.width());
    EXPECT_LE(panel->geometry().bottom(), sidebar.searchField()->mapTo(&sidebar, QPoint(0, 0)).y());
}

} // namespace
