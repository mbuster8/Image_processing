#include "activity_log.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIcon>
#include <QModelIndex>
#include <QString>
#include <QTemporaryDir>
#include <QTime>

#include <gtest/gtest.h>

#include <chrono>

namespace {

using std::chrono::microseconds;
using std::chrono::system_clock;

// A time with a microsecond part, which system_clock keeps on every platform.
system_clock::time_point timeAt(long long microsecondsSinceEpoch) {
    return system_clock::time_point(std::chrono::duration_cast<system_clock::duration>(
        microseconds(microsecondsSinceEpoch)));
}

LogEntry entry(int number, LogKind kind = LogKind::Info) {
    return {timeAt(1'793'000'000'000'000LL + number), kind, QStringLiteral("entry %1").arg(number)};
}

QString textAt(const ActivityLog& log, int row) {
    return log.data(log.index(row), Qt::DisplayRole).toString().section(u' ', 2);
}

int lineCount(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return -1;
    return static_cast<int>(file.readAll().count('\n'));
}

TEST(ActivityLog, FormatsMicroseconds) {
    const QDateTime local(QDate(2026, 10, 27), QTime(13, 2, 44));
    const auto time = timeAt(local.toMSecsSinceEpoch() * 1000 + 907'113);

    EXPECT_EQ(formatLogTime(time), QStringLiteral("2026-10-27 13:02:44.907113"));
}

TEST(ActivityLog, ShowsTimeAndTextWithAKindIcon) {
    ActivityLog log;
    const LogEntry warning = entry(1, LogKind::Warning);

    log.append(warning);

    EXPECT_EQ(log.data(log.index(0), Qt::DisplayRole).toString(),
              formatLogTime(warning.time) + QStringLiteral(" entry 1"));
    EXPECT_FALSE(log.data(log.index(0), Qt::DecorationRole).value<QIcon>().isNull());
    ASSERT_NE(log.latest(), nullptr);
    EXPECT_EQ(log.latest()->kind, LogKind::Warning);
}

TEST(ActivityLog, KeepsNewestThousand) {
    const QTemporaryDir folder;
    const QString path = QDir(folder.path()).filePath(QStringLiteral("activity.log"));
    {
        ActivityLog log(path);
        for (int number = 1; number <= 1005; ++number)
            log.append(entry(number));

        EXPECT_EQ(log.rowCount(), ActivityLog::kMaxEntries);
        EXPECT_EQ(textAt(log, 0), QStringLiteral("entry 1005"));
        EXPECT_EQ(textAt(log, ActivityLog::kMaxEntries - 1), QStringLiteral("entry 6"));
        EXPECT_EQ(log.latest()->text, QStringLiteral("entry 1005"));
    }
    // The file still holds all 1005 lines: loading keeps the newest thousand.
    const ActivityLog reloaded(path);
    EXPECT_EQ(reloaded.rowCount(), ActivityLog::kMaxEntries);
    EXPECT_EQ(textAt(reloaded, 0), QStringLiteral("entry 1005"));
    EXPECT_EQ(textAt(reloaded, ActivityLog::kMaxEntries - 1), QStringLiteral("entry 6"));
}

// The newest entry is the first row, so it is read first.
TEST(ActivityLog, ListsNewestFirst) {
    ActivityLog log;
    log.append(entry(1));
    log.append(entry(2));

    EXPECT_EQ(textAt(log, 0), QStringLiteral("entry 2"));
    EXPECT_EQ(textAt(log, 1), QStringLiteral("entry 1"));
    EXPECT_EQ(log.entryAt(0), log.latest());
}

TEST(ActivityLog, CopiesSelectedRowsOrAll) {
    ActivityLog log;
    for (int number = 1; number <= 3; ++number)
        log.append(entry(number));
    const QString newest = log.data(log.index(0), Qt::DisplayRole).toString();
    const QString middle = log.data(log.index(1), Qt::DisplayRole).toString();
    const QString oldest = log.data(log.index(2), Qt::DisplayRole).toString();
    ASSERT_TRUE(newest.endsWith(QStringLiteral("entry 3")));

    // In the order shown, newest first, whatever order they were selected in.
    EXPECT_EQ(log.toPlainText({log.index(2), log.index(0)}), newest + u'\n' + oldest);
    EXPECT_EQ(log.toPlainText({}), newest + u'\n' + middle + u'\n' + oldest);
}

TEST(ActivityLog, FileRoundTrip) {
    const QTemporaryDir folder;
    const QString path = QDir(folder.path()).filePath(QStringLiteral("activity.log"));
    const LogEntry written[] = {
        {timeAt(1'793'000'000'123'456LL), LogKind::Info, QStringLiteral("tab\there")},
        {timeAt(1'793'000'000'123'457LL), LogKind::Warning, QStringLiteral("line\nbreak")},
        {timeAt(1'793'000'000'999'999LL), LogKind::Error, QStringLiteral("back\\slash \\n \\t")},
        {timeAt(1'793'000'001'000'000LL), LogKind::Info, QStringLiteral("ž😀")},
    };
    {
        ActivityLog log(path);
        for (const LogEntry& entry : written)
            log.append(entry);
    }

    const ActivityLog reloaded(path);

    ASSERT_EQ(reloaded.rowCount(), 4);
    for (int row = 0; row < 4; ++row) {
        SCOPED_TRACE(row);
        const LogEntry& entry = *reloaded.entryAt(row);
        const LogEntry& expected = written[3 - row]; // newest first
        EXPECT_EQ(entry.time, expected.time);
        EXPECT_EQ(entry.kind, expected.kind);
        EXPECT_EQ(entry.text, expected.text);
    }
}

TEST(ActivityLog, CompactsTheFile) {
    const QTemporaryDir folder;
    const QString path = QDir(folder.path()).filePath(QStringLiteral("activity.log"));
    ActivityLog log(path);

    for (int number = 1; number <= 1100; ++number)
        log.append(entry(number));
    EXPECT_EQ(lineCount(path), 1100);
    log.append(entry(1101));

    EXPECT_EQ(lineCount(path), ActivityLog::kMaxEntries);
    const ActivityLog reloaded(path);
    EXPECT_EQ(textAt(reloaded, ActivityLog::kMaxEntries - 1), QStringLiteral("entry 102"));
    EXPECT_EQ(reloaded.latest()->text, QStringLiteral("entry 1101"));
}

// Images may be private, and the log names them.
TEST(ActivityLog, KeepsTheFileOwnerOnly) {
    const QTemporaryDir folder;
    const QString path = QDir(folder.path()).filePath(QStringLiteral("activity.log"));
    constexpr QFileDevice::Permissions kOthers = QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                                 QFileDevice::ReadOther | QFileDevice::WriteOther;
    ActivityLog log(path);

    log.append(entry(1));
    EXPECT_FALSE(QFileInfo(path).permissions() & kOthers);
    for (int number = 2; number <= 1101; ++number)
        log.append(entry(number));

    EXPECT_EQ(lineCount(path), ActivityLog::kMaxEntries); // rewritten
    EXPECT_FALSE(QFileInfo(path).permissions() & kOthers);
}

TEST(ActivityLog, SkipsDamagedLines) {
    const QTemporaryDir folder;
    const QString path = QDir(folder.path()).filePath(QStringLiteral("activity.log"));
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("1793000000000001\tinfo\tfirst\n"
                   "garbage\n"
                   "1793000000000002\tnotice\tunknown kind\n"
                   "soon\tinfo\tno time\n"
                   "1793000000000003\tinfo\tbad \\q escape\n"
                   "1793000000000004\tinfo\ttoo\tmany fields\n"
                   "99999999999999999999\tinfo\tbeyond the clock\n"
                   "1793000000000005\terror\tlast\n");
    }

    const ActivityLog log(path);

    ASSERT_EQ(log.rowCount(), 2);
    EXPECT_EQ(log.entryAt(0)->text, QStringLiteral("last"));
    EXPECT_EQ(log.entryAt(0)->kind, LogKind::Error);
    EXPECT_EQ(log.entryAt(1)->text, QStringLiteral("first"));
}

TEST(ActivityLog, ClearEmptiesModelAndFile) {
    const QTemporaryDir folder;
    const QString path = QDir(folder.path()).filePath(QStringLiteral("activity.log"));
    ActivityLog log(path);
    log.append(entry(1));
    log.append(entry(2));

    log.clear();

    EXPECT_EQ(log.rowCount(), 0);
    EXPECT_EQ(log.latest(), nullptr);
    EXPECT_EQ(ActivityLog(path).rowCount(), 0);
    log.append(entry(3));
    EXPECT_EQ(ActivityLog(path).rowCount(), 1);
}

// A log that cannot be written says so once, in memory, and keeps logging there.
TEST(ActivityLog, ReportsAnUnwritableFileOnce) {
    const QTemporaryDir folder;
    ActivityLog log(QDir(folder.path()).filePath(QStringLiteral("missing/activity.log")));

    log.append(entry(1));
    log.append(entry(2));

    ASSERT_EQ(log.rowCount(), 3);
    EXPECT_EQ(log.entryAt(1)->kind, LogKind::Warning);
    EXPECT_TRUE(log.entryAt(1)->text.startsWith(QStringLiteral("Cannot write the activity log")));
    EXPECT_EQ(log.latest()->text, QStringLiteral("entry 2"));
}

} // namespace
