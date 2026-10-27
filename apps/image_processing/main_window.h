#pragma once

#include "qt_image_sink.h"

#include "viewer_core/image_history.h"
#include "visual_debug/server.h"

#include <QMainWindow>

#include <memory>
#include <optional>
#include <string>

class ImageView;
class QLabel;
class ThumbnailPanel;

namespace Ui {
class MainWindow;
}

// The viewer: received and opened images go into one history, shown as thumbnails and in a zoomable view,
// with the original value of the pixel under the cursor in the status bar.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(visual_debug::ServerOptions serverOptions, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void startServer(bool allowRemote);
    void onImageReceived(const visual_debug::ImageMessage& message);
    bool addImage(std::string name, cv::Mat image);
    void showImage(viewer_core::ImageId id);
    void showPixel(int x, int y);
    void openFile();
    void saveDisplayedImage();

    std::unique_ptr<Ui::MainWindow> m_ui;
    viewer_core::ImageHistory m_history;
    // Declared before the server, so the server's thread has stopped before the sink is destroyed.
    QtImageSink m_sink;
    visual_debug::Server m_server;
    ThumbnailPanel* m_thumbnails = nullptr;
    ImageView* m_imageView = nullptr;
    QLabel* m_pixelLabel = nullptr;
    QLabel* m_serverLabel = nullptr;
    std::optional<viewer_core::ImageId> m_currentId;
};
