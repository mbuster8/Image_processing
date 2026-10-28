#include "activity_log_panel.h"

#include "activity_log.h"
#include "control_checks.h"

#include <QApplication>
#include <QListView>
#include <QPoint>
#include <QScrollBar>
#include <QString>
#include <QToolButton>

#include <gtest/gtest.h>

#include <chrono>

namespace {

void appendEntries(ActivityLog& log, int count) {
    for (int i = 0; i < count; ++i) {
        log.append({std::chrono::system_clock::now(), LogKind::Info, QStringLiteral("entry %1").arg(i)});
    }
}

QString topVisibleText(QListView& list) {
    return list.indexAt(QPoint(1, 1)).data(Qt::DisplayRole).toString();
}

// New rows appear at the top and stay in view while the list is at the top; scrolled down, the rows being
// read stay where they are.
TEST(ActivityLogPanel, FollowsNewRowsOnlyAtTheTop) {
    ActivityLog log;
    ActivityLogPanel panel(&log);
    panel.resize(400, 120);
    panel.show();
    appendEntries(log, 50);
    QApplication::processEvents();
    auto* list = panel.findChild<QListView*>(QStringLiteral("logList"));
    QScrollBar* bar = list->verticalScrollBar();
    ASSERT_GT(bar->maximum(), 0);
    EXPECT_EQ(bar->value(), 0);
    EXPECT_TRUE(topVisibleText(*list).endsWith(QStringLiteral("entry 49")));

    appendEntries(log, 5);
    QApplication::processEvents();
    EXPECT_EQ(bar->value(), 0);
    EXPECT_TRUE(topVisibleText(*list).endsWith(QStringLiteral("entry 4"))); // the newest of the five

    bar->setValue(bar->maximum() / 2);
    QApplication::processEvents();
    const QString reading = topVisibleText(*list);
    appendEntries(log, 5);
    QApplication::processEvents();
    EXPECT_EQ(topVisibleText(*list), reading);
}

TEST(ActivityLogPanel, ButtonsAreLargeEnoughToSee) {
    ActivityLog log;
    ActivityLogPanel panel(&log);
    panel.show();

    EXPECT_TRUE(test_support::isLargeFramedButton(panel.findChild<QToolButton*>(QStringLiteral("logCopy"))));
    EXPECT_TRUE(test_support::isLargeFramedButton(panel.findChild<QToolButton*>(QStringLiteral("logClear"))));
}

} // namespace
