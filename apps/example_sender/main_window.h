#pragma once

#include "send_counter.h"

#include "visual_debug/image_sender.h"

#include <QMainWindow>
#include <QPixmap>
#include <QString>

#include <opencv2/core/mat.hpp>

#include <functional>
#include <memory>
#include <string>

class QLineEdit;

namespace Ui {
class MainWindow;
}

// A minimal host application: open an image, send it to the viewer, see the result. It depends only on the
// ImageSender interface, so it never knows which transport carries the image.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // Makes the sender for an endpoint; called again whenever the endpoint field changes.
    using SenderFactory =
        std::function<std::unique_ptr<visual_debug::ImageSender>(const std::string& endpoint)>;

    explicit MainWindow(SenderFactory makeSender, QWidget* parent = nullptr);
    ~MainWindow() override;

    void openImage(const QString& path);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void openFile();
    void sendImage();
    void showPreview();
    void markEndpointInvalid(bool invalid);

    std::unique_ptr<Ui::MainWindow> m_ui;
    QLineEdit* m_endpointField;
    SenderFactory m_makeSender;
    std::unique_ptr<visual_debug::ImageSender> m_sender;
    std::string m_senderEndpoint; // what m_sender sends to
    cv::Mat m_image;              // as read, sent unchanged
    QString m_fileName;
    QPixmap m_preview;
    SendCounter m_counter;
};
