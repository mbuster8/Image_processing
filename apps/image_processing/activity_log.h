#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QIcon>
#include <QModelIndexList>
#include <QString>

#include <array>
#include <chrono>
#include <deque>

enum class LogKind { Info, Warning, Error };

struct LogEntry {
    std::chrono::system_clock::time_point time;
    LogKind kind = LogKind::Info;
    QString text;
};

// Local date and time with microseconds, "2026-10-27 13:02:44.907113". QDateTime alone has only milliseconds.
QString formatLogTime(std::chrono::system_clock::time_point time);

// What the viewer did, cut to the newest kMaxEntries. Row 0 is the newest entry, so the list reads from the
// top. With a file, it loads the file's newest entries and appends each new one, oldest first in the file, so
// the log survives restarts.
//
// File lines are "<microseconds since the epoch>\t<info|warning|error>\t<text>", with "\", tab and newline in
// the text written as "\\", "\t" and "\n". Lines that do not parse are skipped. Past kMaxFileLines lines the
// file is rewritten with the newest entries, so it never grows without bound.
class ActivityLog : public QAbstractListModel {
    Q_OBJECT

public:
    static constexpr int kMaxEntries = 1000;
    static constexpr int kMaxFileLines = 1100;

    explicit ActivityLog(QString filePath = {}, QObject* parent = nullptr); // empty path: memory only

    // DisplayRole: "<formatLogTime> <text>"; DecorationRole: the kind's icon.
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void append(LogEntry entry); // drops the oldest beyond kMaxEntries
    void clear();                // the model and the file
    // The rows' display text, one per line in row order (newest first); every row when `rows` is empty.
    QString toPlainText(const QModelIndexList& rows) const;
    const LogEntry* entryAt(int row) const; // null outside the rows
    const LogEntry* latest() const;         // null when empty

private:
    void load();
    void appendInMemory(LogEntry entry);
    bool writeLine(const QByteArray& line);
    bool rewriteFile();
    void reportWriteFailure(const QString& reason);

    QString m_filePath;
    std::deque<LogEntry> m_entries; // oldest first, as in the file; rows count from the back
    int m_fileLines = 0;            // lines in the file, damaged ones included
    bool m_writeFailed = false;     // reported once, until a write works again
    std::array<QIcon, 3> m_icons;   // by LogKind
};
