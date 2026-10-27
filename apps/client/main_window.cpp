#include "main_window.h"

#include "image_file.h"
#include "qimage_conversion.h"
#include "ui_main_window.h"

#include "viewer_core/display_converter.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QStatusBar>

#include <string>
#include <utility>
#include <variant>

MainWindow::MainWindow(visual_debug::ImageSender& sender, QWidget* parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()), m_sender(sender) {
    m_ui->setupUi(this);
    connect(m_ui->openButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(m_ui->sendButton, &QPushButton::clicked, this, &MainWindow::sendImage);
}

MainWindow::~MainWindow() = default;

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    showPreview();
}

void MainWindow::openFile() {
    const QString path = qt_common::chooseImageFile(this);
    if (path.isEmpty()) {
        return;
    }
    const QString fileName = QFileInfo(path).fileName();
    auto result = qt_common::readImageFile(path);
    if (const auto* error = std::get_if<QString>(&result)) {
        statusBar()->showMessage(tr("Cannot open %1: %2").arg(fileName, *error));
        return;
    }
    m_image = std::get<cv::Mat>(std::move(result));
    m_fileName = fileName;
    m_preview = QPixmap::fromImage(qt_common::toQImage(viewer_core::toDisplayImage(m_image)));
    m_ui->sendButton->setEnabled(true);
    showPreview();
    statusBar()->showMessage(tr("Opened %1").arg(fileName));
}

// Sends the image as read, not the preview, so the viewer shows its original depth and values.
void MainWindow::sendImage() {
    const std::string name = QStringLiteral("%1 #%2").arg(m_fileName).arg(++m_sentCount).toStdString();
    // Waits for the connection and the reply: about a second at most for a small image, longer for a large
    // one, because the reply deadline grows with the image's size.
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const visual_debug::SendResult result = m_sender.send(m_image, name);
    QGuiApplication::restoreOverrideCursor();
    const std::string_view text = visual_debug::toString(result);
    statusBar()->showMessage(QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())));
}

void MainWindow::showPreview() {
    if (!m_preview.isNull()) {
        m_ui->previewLabel->setPixmap(
            m_preview.scaled(m_ui->previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}
