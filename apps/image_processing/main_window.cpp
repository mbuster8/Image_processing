#include "main_window.h"

#include "image_file.h"
#include "image_view.h"
#include "qimage_conversion.h"
#include "thumbnail_panel.h"
#include "ui_main_window.h"

#include "viewer_core/pixel_inspector.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>

#include <exception>
#include <utility>
#include <variant>

namespace {

constexpr int kStatusMessageMilliseconds = 5000;

QString picturesFolder() { return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation); }

} // namespace

MainWindow::MainWindow(visual_debug::ServerOptions serverOptions, QWidget* parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()), m_server(m_sink, serverOptions) {
    m_ui->setupUi(this);

    m_thumbnails = new ThumbnailPanel;
    m_imageView = new ImageView;
    auto* splitter = new QSplitter;
    splitter->addWidget(m_thumbnails);
    splitter->addWidget(m_imageView);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(1, false);
    setCentralWidget(splitter);

    m_pixelLabel = new QLabel;
    m_pixelLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_serverLabel = new QLabel;
    statusBar()->addWidget(m_pixelLabel, 1);
    statusBar()->addPermanentWidget(m_serverLabel);

    connect(m_ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_ui->actionSaveAs, &QAction::triggered, this, &MainWindow::saveDisplayedImage);
    connect(m_ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(&m_sink, &QtImageSink::imageReceived, this, &MainWindow::onImageReceived, Qt::QueuedConnection);
    connect(m_thumbnails, &ThumbnailPanel::imageSelected, this, &MainWindow::showImage);
    connect(m_imageView, &ImageView::pixelHovered, this, &MainWindow::showPixel);
    connect(m_imageView, &ImageView::hoverLeft, m_pixelLabel, &QLabel::clear);

    // Listen only once the event loop runs: images accepted earlier would wait for a UI that is not yet
    // taking them, and a sender's first burst would find the viewer busy.
    m_serverLabel->setText(tr("Starting…"));
    QTimer::singleShot(0, this, [this, allowRemote = serverOptions.allowRemote] { startServer(allowRemote); });
}

void MainWindow::startServer(bool allowRemote) {
    if (const auto error = m_server.start()) {
        m_serverLabel->setText(tr("Not listening: %1").arg(QString::fromStdString(error->reason)));
        return;
    }
    const QString endpoint = QString::fromStdString(m_server.boundEndpoint());
    m_serverLabel->setText(allowRemote ? tr("Listening on %1 (remote connections allowed)").arg(endpoint)
                                       : tr("Listening on %1").arg(endpoint));
}

// Child widgets outlive this window's members; stop the panel reporting selections into a destroyed history.
MainWindow::~MainWindow() { m_thumbnails->disconnect(this); }

void MainWindow::onImageReceived(const visual_debug::ImageMessage& message) {
    addImage(message.name, message.image);
    m_sink.markHandled();
}

// The one path for received and opened images: into the history, onto the panel (which selects it and so
// shows it), and out with whatever the history evicted.
bool MainWindow::addImage(std::string name, cv::Mat image) {
    viewer_core::ImageHistory::AddResult added;
    try {
        added = m_history.add(std::move(name), std::move(image));
    } catch (const std::exception& error) {
        statusBar()->showMessage(tr("Cannot show image: %1").arg(error.what()), kStatusMessageMilliseconds);
        return false;
    }
    m_thumbnails->addImage(*m_history.find(added.id));
    for (const auto evicted : added.evicted) {
        m_thumbnails->removeImage(evicted);
    }
    return true;
}

void MainWindow::showImage(viewer_core::ImageId id) {
    const auto* entry = m_history.find(id);
    if (entry == nullptr) {
        return;
    }
    m_currentId = id;
    m_imageView->setImage(qt_common::toQImage(entry->display));
    setWindowTitle(tr("Image Processing — %1").arg(QString::fromStdString(entry->name)));
}

void MainWindow::showPixel(int x, int y) {
    const auto* entry = m_currentId ? m_history.find(*m_currentId) : nullptr;
    const auto text = entry != nullptr ? viewer_core::inspectPixel(entry->original, x, y) : std::nullopt;
    m_pixelLabel->setText(text ? QString::fromStdString(*text) : QString());
}

void MainWindow::openFile() {
    const QString path = qt_common::chooseImageFile(this);
    if (path.isEmpty()) {
        return;
    }
    const QString fileName = QFileInfo(path).fileName();
    auto result = qt_common::readImageFile(path);
    if (const auto* error = std::get_if<QString>(&result)) {
        statusBar()->showMessage(tr("Cannot open %1: %2").arg(fileName, *error), kStatusMessageMilliseconds);
        return;
    }
    addImage(fileName.toStdString(), std::get<cv::Mat>(std::move(result)));
}

// Saves what is on screen (the 8-bit display image), not the original: PNG and JPEG cannot hold every
// depth. QImage::save is used because cv::imwrite would swap red and blue in an RGBA image.
void MainWindow::saveDisplayedImage() {
    const auto* entry = m_currentId ? m_history.find(*m_currentId) : nullptr;
    if (entry == nullptr) {
        statusBar()->showMessage(tr("No image to save"), kStatusMessageMilliseconds);
        return;
    }
    QString suggestedName = QString::fromStdString(entry->name);
    suggestedName.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    QString path = QFileDialog::getSaveFileName(this, tr("Save Displayed Image"),
                                                picturesFolder() + '/' + suggestedName + QStringLiteral(".png"),
                                                tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".png");
    }
    const bool saved = qt_common::toQImage(entry->display).save(path);
    statusBar()->showMessage(saved ? tr("Saved %1").arg(path) : tr("Could not save %1").arg(path),
                             kStatusMessageMilliseconds);
}
