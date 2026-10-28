#include "activity_log.h"

#include "epoch_time.h"
#include "outline_icons.h"
#include "private_files.h"

#include <QByteArray>
#include <QByteArrayList>
#include <QDateTime>
#include <QFile>

#include <algorithm>
#include <optional>
#include <utility>

namespace {

using std::chrono::system_clock;

const char* kindName(LogKind kind) {
    switch (kind) {
    case LogKind::Info:
        return "info";
    case LogKind::Warning:
        return "warning";
    case LogKind::Error:
        return "error";
    }
    return "info";
}

std::optional<LogKind> kindFromName(const QByteArray& name) {
    for (const LogKind kind : {LogKind::Info, LogKind::Warning, LogKind::Error}) {
        if (name == kindName(kind)) {
            return kind;
        }
    }
    return std::nullopt;
}

QString escaped(const QString& text) {
    QString result;
    result.reserve(text.size());
    for (const QChar character : text) {
        if (character == u'\\') {
            result += QStringLiteral("\\\\");
        } else if (character == u'\t') {
            result += QStringLiteral("\\t");
        } else if (character == u'\n') {
            result += QStringLiteral("\\n");
        } else {
            result += character;
        }
    }
    return result;
}

std::optional<QString> unescaped(const QString& text) {
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text[i] != u'\\') {
            result += text[i];
            continue;
        }
        if (++i == text.size()) {
            return std::nullopt;
        }
        if (text[i] == u'\\') {
            result += u'\\';
        } else if (text[i] == u't') {
            result += u'\t';
        } else if (text[i] == u'n') {
            result += u'\n';
        } else {
            return std::nullopt;
        }
    }
    return result;
}

QByteArray fileLine(const LogEntry& entry) {
    return QByteArray::number(toEpochMicroseconds(entry.time)) + '\t' + kindName(entry.kind) + '\t' +
           escaped(entry.text).toUtf8() + '\n';
}

std::optional<LogEntry> parseLine(QByteArray line) {
    if (line.endsWith('\n')) {
        line.chop(1);
    }
    const QByteArrayList fields = line.split('\t');
    if (fields.size() != 3) {
        return std::nullopt;
    }
    bool isNumber = false;
    const qint64 sinceEpoch = fields[0].toLongLong(&isNumber);
    const auto time = isNumber ? fromEpochMicroseconds(sinceEpoch) : std::nullopt;
    const std::optional<LogKind> kind = kindFromName(fields[1]);
    std::optional<QString> text = unescaped(QString::fromUtf8(fields[2]));
    if (!time || !kind || !text) {
        return std::nullopt;
    }
    return LogEntry{*time, *kind, std::move(*text)};
}

} // namespace

QString formatLogTime(system_clock::time_point time) {
    const std::chrono::microseconds sinceEpoch{toEpochMicroseconds(time)};
    const auto seconds = std::chrono::floor<std::chrono::seconds>(sinceEpoch);
    const auto fraction = (sinceEpoch - seconds).count();
    return QDateTime::fromSecsSinceEpoch(seconds.count()).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) +
           u'.' + QStringLiteral("%1").arg(fraction, 6, 10, QLatin1Char('0'));
}

ActivityLog::ActivityLog(QString filePath, QObject* parent)
    : QAbstractListModel(parent), m_filePath(std::move(filePath)),
      m_icons{qt_common::makeOutlineIcon(qt_common::OutlineIcon::Info),
              qt_common::makeOutlineIcon(qt_common::OutlineIcon::Warning),
              qt_common::makeOutlineIcon(qt_common::OutlineIcon::Error)} {
    load();
}

int ActivityLog::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ActivityLog::data(const QModelIndex& index, int role) const {
    const LogEntry* entry = entryAt(index.row());
    if (!index.isValid() || entry == nullptr) {
        return {};
    }
    switch (role) {
    case Qt::DisplayRole:
        return QString(formatLogTime(entry->time) + u' ' + entry->text);
    case Qt::DecorationRole:
        return m_icons[static_cast<std::size_t>(entry->kind)];
    default:
        return {};
    }
}

void ActivityLog::append(LogEntry entry) {
    const QByteArray line = m_filePath.isEmpty() ? QByteArray() : fileLine(entry);
    // In memory first, so a failure to write shows after the entry it concerns.
    appendInMemory(std::move(entry));
    if (m_filePath.isEmpty() || !writeLine(line)) {
        return;
    }
    m_writeFailed = false;
    // A failed rewrite tries again after another kMaxFileLines lines, not on every append.
    if (++m_fileLines > kMaxFileLines && !rewriteFile()) {
        m_fileLines = 0;
    }
}

void ActivityLog::clear() {
    beginResetModel();
    m_entries.clear();
    endResetModel();
    if (!m_filePath.isEmpty() && QFile::exists(m_filePath) && !QFile::remove(m_filePath)) {
        reportWriteFailure(tr("cannot remove the file"));
    }
    m_fileLines = 0;
}

QString ActivityLog::toPlainText(const QModelIndexList& rows) const {
    std::vector<int> numbers;
    if (rows.isEmpty()) {
        for (int row = 0; row < rowCount(); ++row) {
            numbers.push_back(row);
        }
    } else {
        for (const QModelIndex& index : rows) {
            numbers.push_back(index.row());
        }
        std::sort(numbers.begin(), numbers.end());
    }
    QStringList lines;
    for (const int row : numbers) {
        lines.append(data(index(row), Qt::DisplayRole).toString());
    }
    return lines.join(u'\n');
}

const LogEntry* ActivityLog::entryAt(int row) const {
    return row >= 0 && row < rowCount() ? &m_entries[m_entries.size() - 1 - static_cast<std::size_t>(row)]
                                        : nullptr;
}

const LogEntry* ActivityLog::latest() const { return m_entries.empty() ? nullptr : &m_entries.back(); }

void ActivityLog::load() {
    if (m_filePath.isEmpty()) {
        return;
    }
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return; // the first run has no file; a file that cannot be read shows up at the first write
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        ++m_fileLines;
        if (std::optional<LogEntry> entry = parseLine(line)) {
            m_entries.push_back(std::move(*entry));
            if (m_entries.size() > kMaxEntries) {
                m_entries.pop_front();
            }
        }
    }
}

void ActivityLog::appendInMemory(LogEntry entry) {
    if (m_entries.size() >= kMaxEntries) {
        const int oldest = rowCount() - 1;
        beginRemoveRows({}, oldest, oldest);
        m_entries.pop_front();
        endRemoveRows();
    }
    beginInsertRows({}, 0, 0);
    m_entries.push_back(std::move(entry));
    endInsertRows();
}

bool ActivityLog::writeLine(const QByteArray& line) {
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append, kOwnerOnlyFile) ||
        file.write(line) != line.size()) {
        reportWriteFailure(file.errorString());
        return false;
    }
    return true;
}

bool ActivityLog::rewriteFile() {
    QString error;
    const bool rewritten = writeFileAtomically(
        m_filePath,
        [this](QIODevice& file) {
            for (const LogEntry& entry : m_entries) {
                file.write(fileLine(entry));
            }
            return true;
        },
        &error);
    if (!rewritten) {
        reportWriteFailure(error);
        return false;
    }
    m_fileLines = static_cast<int>(m_entries.size());
    return true;
}

void ActivityLog::reportWriteFailure(const QString& reason) {
    if (std::exchange(m_writeFailed, true)) {
        return;
    }
    // In memory only: writing it would fail too.
    appendInMemory(
        {system_clock::now(), LogKind::Warning, tr("Cannot write the activity log: %1").arg(reason)});
}
