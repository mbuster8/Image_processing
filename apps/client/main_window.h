#pragma once

#include "visual_debug/image_sender.h"

#include <QMainWindow>
#include <QPixmap>
#include <QString>

#include <opencv2/core/mat.hpp>

#include <memory>

namespace Ui {
class MainWindow;
}

// A minimal host application: open an image, send it to the viewer, see the result. It depends only on the
// ImageSender interface, so it never knows which transport carries the image.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(visual_debug::ImageSender& sender, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void openFile();
    void sendImage();
    void showPreview();

    std::unique_ptr<Ui::MainWindow> m_ui;
    visual_debug::ImageSender& m_sender;
    cv::Mat m_image; // as read, sent unchanged
    QString m_fileName;
    QPixmap m_preview;
    int m_sentCount = 0;
};
