#pragma once

#include "viewer_core/image_history.h"

#include <QAbstractListModel>
#include <QCollator>
#include <QFlags>
#include <QPixmap>
#include <QSize>
#include <QSortFilterProxyModel>
#include <QString>

#include <opencv2/core/mat.hpp>

#include <vector>

// What each sidebar item shows. At least one is always on.
enum class SidebarField { Thumbnail = 0x1, Name = 0x2, Details = 0x4 };
Q_DECLARE_FLAGS(SidebarFields, SidebarField)
Q_DECLARE_OPERATORS_FOR_FLAGS(SidebarFields)

// The history as a list: one row per entry, in history order (row 0 is the newest). It only mirrors the
// history; what the sidebar lists, and in which order, is ThumbnailFilterModel's business.
class ThumbnailModel : public QAbstractListModel {
    Q_OBJECT

public:
    // Qt::DisplayRole is the caption lines shown (name, details, either or neither), joined by '\n'.
    // Qt::ToolTipRole holds everything, so hidden text is one hover away: name, details, received time, and
    // the modified time when it differs. IdRole to ModifiedRole are the keys the sidebar sorts by: the name
    // alone, pixelDataBytes(), and the modified time in microseconds since the epoch. DetailsRole
    // (imageDetails()) and ImageSizeRole (columns × rows) are for drawing.
    enum Role { IdRole = Qt::UserRole + 1, NameRole, BytesRole, ModifiedRole, DetailsRole, ImageSizeRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // `row` is the entry's history index, clamped to the rows there are.
    void insertImage(const viewer_core::ReceivedImage& image, int row);
    void removeImage(viewer_core::ImageId id);
    // Refreshes the name, the details, the times and the sort keys, after a rename.
    void updateImage(const viewer_core::ReceivedImage& image);
    QModelIndex indexOf(viewer_core::ImageId id) const; // invalid when the id has no row

    void setShownFields(SidebarFields fields); // an empty set is ignored
    SidebarFields shownFields() const;

    // The image scaled to exactly `deviceSize` pixels. Each row keeps the last one it made, so painting at an
    // unchanged size costs nothing; a new width or screen makes a new one. Null for an unknown id.
    QPixmap thumbnail(viewer_core::ImageId id, QSize deviceSize) const;

private:
    struct Row {
        viewer_core::ImageId id = 0;
        QString name;
        QString details;
        qulonglong bytes = 0;
        qint64 receivedMicroseconds = 0;
        qint64 modifiedMicroseconds = 0;
        cv::Mat display; // shares the history's pixels
        mutable QPixmap thumbnail;
    };

    void setTexts(Row& row, const viewer_core::ReceivedImage& image);
    QString caption(const Row& row) const;
    static QString toolTip(const Row& row);
    int rowOf(viewer_core::ImageId id) const; // -1 when absent

    std::vector<Row> m_rows;
    SidebarFields m_fields = SidebarField::Thumbnail | SidebarField::Name | SidebarField::Details;
};

// The keys the sidebar sorts by. Received is the history's own order.
enum class SidebarSort { Received, Modified, Name, Size };

// What the sidebar lists, in the order it lists it: the names that contain the search text, sorted by one
// key. Rows that arrive or change are filtered and sorted at once.
class ThumbnailFilterModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit ThumbnailFilterModel(QObject* parent = nullptr);

    void setSearchText(const QString& text); // ignoring case, anywhere in the name; "" lists every row
    void setSort(SidebarSort key, Qt::SortOrder order);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_searchText;
    SidebarSort m_sortKey = SidebarSort::Received;
    QCollator m_nameCollator; // as Finder sorts names: ignoring case, numbers by value
};
