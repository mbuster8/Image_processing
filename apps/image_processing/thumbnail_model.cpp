#include "thumbnail_model.h"

#include "display_text.h"
#include "epoch_time.h"
#include "qimage_conversion.h"

#include <QStringList>
#include <QVariant>
#include <QtGlobal>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>

int ThumbnailModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ThumbnailModel::data(const QModelIndex& index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row& row = m_rows[static_cast<std::size_t>(index.row())];
    switch (role) {
    case Qt::DisplayRole:
        return caption(row);
    case Qt::ToolTipRole:
        return toolTip(row);
    case NameRole:
        return row.name;
    case IdRole:
        return QVariant::fromValue<quint64>(row.id);
    case BytesRole:
        return row.bytes;
    case ModifiedRole:
        return row.modifiedMicroseconds;
    case DetailsRole:
        return row.details;
    case ImageSizeRole:
        return QSize(row.display.cols, row.display.rows);
    default:
        return {};
    }
}

void ThumbnailModel::insertImage(const viewer_core::ReceivedImage& image, int row) {
    row = std::clamp(row, 0, rowCount());
    beginInsertRows(QModelIndex(), row, row);
    Row entry;
    entry.id = image.id;
    entry.display = image.display;
    setTexts(entry, image);
    m_rows.insert(m_rows.begin() + row, std::move(entry));
    endInsertRows();
}

void ThumbnailModel::removeImage(viewer_core::ImageId id) {
    const int row = rowOf(id);
    if (row < 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_rows.erase(m_rows.begin() + row);
    endRemoveRows();
}

void ThumbnailModel::updateImage(const viewer_core::ReceivedImage& image) {
    const int row = rowOf(image.id);
    if (row < 0) {
        return;
    }
    setTexts(m_rows[static_cast<std::size_t>(row)], image);
    emit dataChanged(index(row), index(row));
}

QModelIndex ThumbnailModel::indexOf(viewer_core::ImageId id) const {
    const int row = rowOf(id);
    return row < 0 ? QModelIndex() : index(row);
}

void ThumbnailModel::setShownFields(SidebarFields fields) {
    if (fields == SidebarFields() || fields == m_fields) {
        return;
    }
    m_fields = fields;
    if (!m_rows.empty()) {
        emit dataChanged(index(0), index(rowCount() - 1), {Qt::DisplayRole});
    }
}

SidebarFields ThumbnailModel::shownFields() const { return m_fields; }

QPixmap ThumbnailModel::thumbnail(viewer_core::ImageId id, QSize deviceSize) const {
    const int row = rowOf(id);
    if (row < 0 || deviceSize.isEmpty()) {
        return {};
    }
    const Row& entry = m_rows[static_cast<std::size_t>(row)];
    if (entry.thumbnail.size() != deviceSize) {
        // Area averaging when shrinking; nearest neighbour when enlarging, so a tiny image stays sharp.
        const bool shrinking =
            deviceSize.width() < entry.display.cols || deviceSize.height() < entry.display.rows;
        cv::Mat scaled;
        cv::resize(entry.display, scaled, cv::Size(deviceSize.width(), deviceSize.height()), 0, 0,
                   shrinking ? cv::INTER_AREA : cv::INTER_NEAREST);
        entry.thumbnail = QPixmap::fromImage(qt_common::toQImage(scaled));
    }
    return entry.thumbnail;
}

void ThumbnailModel::setTexts(Row& row, const viewer_core::ReceivedImage& image) {
    row.name = QString::fromStdString(image.name);
    row.details = imageDetails(image);
    row.bytes = image.pixelDataBytes();
    row.receivedMicroseconds = toEpochMicroseconds(image.origin.arrivedAt);
    row.modifiedMicroseconds = toEpochMicroseconds(image.origin.modifiedAt);
}

QString ThumbnailModel::caption(const Row& row) const {
    QStringList lines;
    if (m_fields.testFlag(SidebarField::Name)) {
        lines << row.name;
    }
    if (m_fields.testFlag(SidebarField::Details)) {
        lines << row.details;
    }
    return lines.join(QLatin1Char('\n'));
}

QString ThumbnailModel::toolTip(const Row& row) {
    QStringList lines{row.name, row.details,
                      tr("Received %1").arg(clockTime(row.receivedMicroseconds))};
    if (row.modifiedMicroseconds != row.receivedMicroseconds) {
        lines << tr("Modified %1").arg(clockTime(row.modifiedMicroseconds));
    }
    return lines.join(QLatin1Char('\n'));
}

int ThumbnailModel::rowOf(viewer_core::ImageId id) const {
    const auto found =
        std::find_if(m_rows.begin(), m_rows.end(), [id](const Row& row) { return row.id == id; });
    return found == m_rows.end() ? -1 : static_cast<int>(found - m_rows.begin());
}

ThumbnailFilterModel::ThumbnailFilterModel(QObject* parent) : QSortFilterProxyModel(parent) {
    m_nameCollator.setNumericMode(true);
    m_nameCollator.setCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void ThumbnailFilterModel::setSearchText(const QString& text) {
    if (text == m_searchText) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_searchText = text;
    endFilterChange(Direction::Rows);
#else
    m_searchText = text;
    invalidateRowsFilter();
#endif
}

void ThumbnailFilterModel::setSort(SidebarSort key, Qt::SortOrder order) {
    const bool keyChanged = key != m_sortKey;
    m_sortKey = key;
    // Sorted also by Received, so that dynamic sorting places arrivals. sort() does nothing for an unchanged
    // column and order, so a new key alone needs invalidate().
    if (keyChanged && sortColumn() == 0 && sortOrder() == order) {
        invalidate();
    } else {
        sort(0, order);
    }
}

bool ThumbnailFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    return m_searchText.isEmpty() || sourceModel()
                                         ->index(sourceRow, 0, sourceParent)
                                         .data(ThumbnailModel::NameRole)
                                         .toString()
                                         .contains(m_searchText, Qt::CaseInsensitive);
}

bool ThumbnailFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    // Source row 0 is the newest. Equal keys keep the newest first in both directions: the proxy reverses
    // this comparison for Descending, so the tie-break reverses with it.
    const bool newerFirst =
        sortOrder() == Qt::AscendingOrder ? left.row() < right.row() : left.row() > right.row();
    switch (m_sortKey) {
    case SidebarSort::Received:
        return left.row() > right.row(); // a later row arrived earlier
    case SidebarSort::Modified: {
        const qint64 leftTime = left.data(ThumbnailModel::ModifiedRole).toLongLong();
        const qint64 rightTime = right.data(ThumbnailModel::ModifiedRole).toLongLong();
        return leftTime != rightTime ? leftTime < rightTime : newerFirst;
    }
    case SidebarSort::Name: {
        const int order = m_nameCollator.compare(left.data(ThumbnailModel::NameRole).toString(),
                                                 right.data(ThumbnailModel::NameRole).toString());
        return order != 0 ? order < 0 : newerFirst;
    }
    case SidebarSort::Size: {
        const qulonglong leftBytes = left.data(ThumbnailModel::BytesRole).toULongLong();
        const qulonglong rightBytes = right.data(ThumbnailModel::BytesRole).toULongLong();
        return leftBytes != rightBytes ? leftBytes < rightBytes : newerFirst;
    }
    }
    return newerFirst;
}
