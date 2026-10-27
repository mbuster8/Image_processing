#pragma once

#include "viewer_core/image_history.h"

#include <QListWidget>
#include <QSize>

// The history down the side of the viewer: newest first, each image as a thumbnail with its name and summary.
class ThumbnailPanel : public QListWidget {
    Q_OBJECT

public:
    static constexpr QSize kThumbnailSize{150, 90};

    explicit ThumbnailPanel(QWidget* parent = nullptr);

    // Adds the image at the top and selects it.
    void addImage(const viewer_core::ReceivedImage& image);
    void removeImage(viewer_core::ImageId id);

signals:
    void imageSelected(viewer_core::ImageId id);
};
