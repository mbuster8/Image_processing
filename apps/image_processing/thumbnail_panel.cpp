#include "thumbnail_panel.h"

#include "qimage_conversion.h"

#include <QIcon>
#include <QListWidgetItem>
#include <QPixmap>
#include <QScrollBar>
#include <QVariant>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kItemPadding = 12;
constexpr int kCaptionLines = 2;

viewer_core::ImageId idOf(const QListWidgetItem* item) { return item->data(Qt::UserRole).value<quint64>(); }

// Fits the image into kThumbnailSize, keeping its aspect ratio: area averaging when shrinking, nearest
// neighbour when enlarging so a tiny image stays sharp.
cv::Mat makeThumbnail(const cv::Mat& display) {
    const double scale = std::min(static_cast<double>(ThumbnailPanel::kThumbnailSize.width()) / display.cols,
                                  static_cast<double>(ThumbnailPanel::kThumbnailSize.height()) / display.rows);
    const cv::Size size(std::max(1, static_cast<int>(std::lround(display.cols * scale))),
                        std::max(1, static_cast<int>(std::lround(display.rows * scale))));
    cv::Mat thumbnail;
    cv::resize(display, thumbnail, size, 0, 0, scale < 1.0 ? cv::INTER_AREA : cv::INTER_NEAREST);
    return thumbnail;
}

} // namespace

ThumbnailPanel::ThumbnailPanel(QWidget* parent) : QListWidget(parent) {
    setViewMode(QListView::IconMode);
    setFlow(QListView::TopToBottom);
    setWrapping(false);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setTextElideMode(Qt::ElideMiddle);
    setIconSize(kThumbnailSize);
    setGridSize({kThumbnailSize.width() + 2 * kItemPadding,
                 kThumbnailSize.height() + kItemPadding + kCaptionLines * fontMetrics().lineSpacing()});
    setFixedWidth(gridSize().width() + 2 * frameWidth() + verticalScrollBar()->sizeHint().width());

    connect(this, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current) {
        if (current != nullptr) {
            emit imageSelected(idOf(current));
        }
    });
}

void ThumbnailPanel::addImage(const viewer_core::ReceivedImage& image) {
    const QPixmap thumbnail = QPixmap::fromImage(qt_common::toQImage(makeThumbnail(image.display)));
    auto* item = new QListWidgetItem(QIcon(thumbnail), QString::fromStdString(image.name + '\n' + image.summary));
    item->setData(Qt::UserRole, QVariant::fromValue<quint64>(image.id));
    item->setToolTip(QString::fromStdString(image.name));
    insertItem(0, item);
    setCurrentItem(item);
}

void ThumbnailPanel::removeImage(viewer_core::ImageId id) {
    for (int row = 0; row < count(); ++row) {
        if (idOf(item(row)) == id) {
            delete takeItem(row);
            return;
        }
    }
}
