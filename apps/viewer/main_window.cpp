#include "main_window.h"

#include "about.h"
#include "activity_log_panel.h"
#include "appearance.h"
#include "application_identity.h"
#include "controls.h"
#include "display_text.h"
#include "divider_splitter.h"
#include "eliding_label.h"
#include "epoch_time.h"
#include "image_file.h"
#include "image_info_bar.h"
#include "image_sidebar.h"
#include "image_view.h"
#include "outline_icons.h"
#include "qimage_conversion.h"
#include "shades.h"
#include "thumbnail_panel.h"
#include "ui_main_window.h"

#include "viewer_core/pixel_inspector.h"
#include "viewer_core/save_name.h"
#include "visual_debug/wire_format.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iterator>
#include <utility>
#include <variant>

namespace {

constexpr int kImageViewSizePlaceholder = 1000;
constexpr int kDefaultLogHeight = 160;
const QString kSidebarWidthKey = QStringLiteral("viewer/sidebarWidth");
const QString kLogOpenKey = QStringLiteral("viewer/logOpen");
const QString kLogHeightKey = QStringLiteral("viewer/logHeight");

QString picturesFolder() { return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation); }

bool isTextField(const QWidget* widget) {
    return qobject_cast<const QLineEdit*>(widget) != nullptr ||
           qobject_cast<const QTextEdit*>(widget) != nullptr ||
           qobject_cast<const QPlainTextEdit*>(widget) != nullptr;
}

} // namespace

QString viewerWindowTitle(const QString& imageName, const QString& endpoint) {
    const QString tcpScheme = QStringLiteral("tcp://");
    const QString shownEndpoint = endpoint.startsWith(tcpScheme) ? endpoint.mid(tcpScheme.size()) : endpoint;
    return imageName.isEmpty()
               ? QCoreApplication::translate("MainWindow", "Visual Debug Viewer — %1").arg(shownEndpoint)
               : QCoreApplication::translate("MainWindow", "Visual Debug Viewer — %1 — %2")
                     .arg(imageName, shownEndpoint);
}

MainWindow::MainWindow(visual_debug::ServerOptions serverOptions, const QString& dataRoot, QWidget* parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()), m_server(m_sink, serverOptions),
      m_endpoint(QString::fromStdString(serverOptions.endpoint)), m_allowRemote(serverOptions.allowRemote) {
    m_ui->setupUi(this);
    setWindowTitle(viewerWindowTitle(QString(), m_endpoint));

    DataFolder::Problem dataProblem = DataFolder::Problem::None;
    m_dataFolder = DataFolder::open(dataRoot, m_endpoint, &dataProblem);
    m_log = new ActivityLog(m_dataFolder ? m_dataFolder->logFile() : QString(), this);
    m_dataRoot = dataRoot;
    m_storage = std::make_unique<StorageController>(m_dataFolder ? m_dataFolder->imagesFolder() : QString());
    connect(m_storage.get(), &StorageController::restoreFinished, this, &MainWindow::onRestoreFinished);
    connect(m_storage.get(), &StorageController::failed, this,
            [this](const QString& what) { log(LogKind::Error, what); });
    // Leftovers of viewers that crashed while the setting was on. A running viewer's folder is its own to
    // clean, at its next write.
    if (!keepImagesSetting()) {
        DataFolder::removeStoredImagesEverywhere(dataRoot, true);
    }

    m_sidebar = new ImageSidebar;
    m_thumbnails = m_sidebar->thumbnails();
    m_infoBar = new ImageInfoBar;
    m_imageView = new ImageView;
    // The info strip sits on top of the image, in the splitter's second section.
    auto* imageArea = new QWidget;
    imageArea->setObjectName(QStringLiteral("imageArea"));
    auto* imageLayout = new QVBoxLayout(imageArea);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(0);
    imageLayout->addWidget(m_infoBar);
    imageLayout->addWidget(m_imageView, 1);
    auto* splitter = new DividerSplitter(Qt::Horizontal);
    splitter->setObjectName(QStringLiteral("mainSplitter"));
    splitter->addWidget(m_sidebar);
    splitter->addWidget(imageArea);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    // The image area's size is a placeholder: it takes whatever the sidebar leaves.
    splitter->setSizes({QSettings().value(kSidebarWidthKey, ImageSidebar::kDefaultWidth).toInt(),
                        kImageViewSizePlaceholder});
    connect(splitter, &QSplitter::splitterMoved, this,
            [splitter] { QSettings().setValue(kSidebarWidthKey, splitter->sizes().value(0)); });
    // The log opens below everything else, and only the log keeps its height when the window grows.
    m_logPanel = new ActivityLogPanel(m_log);
    m_logSplitter = new DividerSplitter(Qt::Vertical);
    m_logSplitter->setObjectName(QStringLiteral("logSplitter"));
    m_logSplitter->addWidget(splitter);
    m_logSplitter->addWidget(m_logPanel);
    m_logSplitter->setStretchFactor(0, 1);
    m_logSplitter->setCollapsible(0, false);
    m_logSplitter->setCollapsible(1, false);
    connect(m_logSplitter, &QSplitter::splitterMoved, this, [this] {
        QSettings().setValue(kLogHeightKey, m_logSplitter->sizes().value(1));
    });
    // The divider marks the status bar's top edge.
    auto* central = new QWidget;
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_logSplitter, 1);
    auto* statusBarDivider = new qt_common::DividerLine;
    statusBarDivider->setObjectName(QStringLiteral("statusBarDivider"));
    centralLayout->addWidget(statusBarDivider);
    setCentralWidget(central);
    qt_common::usePanelGrey(m_sidebar);
    qt_common::usePanelGrey(m_logPanel);
    // The keyboard starts on the thumbnails, so the arrow keys step through the images and the search field
    // shows its accent-coloured focus ring only once it is used.
    m_thumbnails->setFocus(Qt::OtherFocusReason);

    m_pixelLabel = new QLabel;
    m_pixelLabel->setObjectName(QStringLiteral("pixelReadout"));
    m_pixelLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // The platform's usual margin, so the readout does not touch the window's left edge.
    m_pixelLabel->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin), 0, 0, 0);
    auto* latestLabel = new ElidingLabel(Qt::ElideRight);
    latestLabel->setObjectName(QStringLiteral("latestLogEntry"));
    latestLabel->setForegroundRole(QPalette::PlaceholderText);
    latestLabel->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin), 0, 0, 0);
    m_latestLabel = latestLabel;
    m_logToggle = new QToolButton;
    m_logToggle->setObjectName(QStringLiteral("logToggle"));
    m_logToggle->setText(tr("Log"));
    m_logToggle->setCheckable(true);
    qt_common::frameButton(m_logToggle);
    m_logToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_logToggle->setLayoutDirection(Qt::RightToLeft); // "Log ▴": the chevron after the word
    m_logToggle->setToolTip(tr("Show or hide the activity log"));
    m_serverControls = new ServerControls;
    statusBar()->setFont(qt_common::regularFont());
    qt_common::usePanelGrey(statusBar());
    statusBar()->addWidget(m_pixelLabel);
    statusBar()->addWidget(m_latestLabel, 1);
    statusBar()->addPermanentWidget(m_serverControls);
    statusBar()->addPermanentWidget(m_logToggle);
    connect(m_logToggle, &QToolButton::toggled, this, &MainWindow::setLogOpen);
    connect(m_log, &QAbstractItemModel::rowsInserted, this, &MainWindow::showLatestEntry);
    connect(m_log, &QAbstractItemModel::modelReset, this, &MainWindow::showLatestEntry);
    setLogOpen(QSettings().value(kLogOpenKey, false).toBool());
    showLatestEntry();

    // The same shortcuts as the thumbnail icons' tooltips name.
    m_ui->actionRename->setShortcut(thumbnailActionShortcut(ThumbnailAction::Rename));
    m_ui->actionDuplicate->setShortcut(thumbnailActionShortcut(ThumbnailAction::Duplicate));
    m_ui->actionCopyImage->setShortcut(thumbnailActionShortcut(ThumbnailAction::CopyImage));
    m_ui->actionDelete->setShortcut(thumbnailActionShortcut(ThumbnailAction::Delete));

    connect(m_ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_ui->actionSaveAs, &QAction::triggered, this, [this] {
        if (m_currentId) {
            saveImage(*m_currentId);
        } else {
            log(LogKind::Warning, tr("No image to save"));
        }
    });
    connect(m_ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(m_ui->actionUndoDelete, &QAction::triggered, this, &MainWindow::undoDelete);
    connect(m_ui->actionFind, &QAction::triggered, m_sidebar, &ImageSidebar::focusSearch);
    m_sidebar->populateViewMenu(m_ui->menuView);
    m_ui->menuView->addSeparator();
    m_ui->menuView->addAction(m_ui->actionShowLog);
    connect(m_ui->actionShowLog, &QAction::triggered, this, &MainWindow::setLogOpen);
    qt_common::addAppearanceMenu(m_ui->menuView);
    qt_common::addHelpMenu(menuBar(), QCoreApplication::translate("About", qt_common::kViewerDescription));
    // Another viewer may have changed the shared setting since.
    const auto showKeepImages = [this] { m_ui->actionKeepImages->setChecked(keepImagesSetting()); };
    showKeepImages();
    connect(m_ui->menuHistory, &QMenu::aboutToShow, this, showKeepImages);
    connect(m_ui->actionKeepImages, &QAction::triggered, this, &MainWindow::setKeepImages);
    connect(m_ui->actionClearHistory, &QAction::triggered, this, &MainWindow::clearHistory);
    // The Edit menu acts on the image in the view.
    const auto onShownImage = [this](void (MainWindow::*act)(viewer_core::ImageId)) {
        return [this, act] {
            if (m_currentId) {
                (this->*act)(*m_currentId);
            }
        };
    };
    connect(m_ui->actionRename, &QAction::triggered, this, onShownImage(&MainWindow::startRename));
    connect(m_ui->actionDuplicate, &QAction::triggered, this, onShownImage(&MainWindow::duplicateImage));
    connect(m_ui->actionCopyImage, &QAction::triggered, this, onShownImage(&MainWindow::copyImage));
    connect(m_ui->actionDelete, &QAction::triggered, this, onShownImage(&MainWindow::deleteImage));
    connect(qApp, &QApplication::focusChanged, this, &MainWindow::updateEditActions);
    connect(m_ui->actionPauseServer, &QAction::triggered, this, &MainWindow::togglePause);
    connect(m_ui->actionStopServer, &QAction::triggered, this, &MainWindow::toggleRunning);
    connect(m_serverControls, &ServerControls::pauseClicked, this, &MainWindow::togglePause);
    connect(m_serverControls, &ServerControls::stopClicked, this, &MainWindow::toggleRunning);

    connect(&m_sink, &QtImageSink::imageReceived, this, &MainWindow::onImageReceived, Qt::QueuedConnection);
    connect(&m_sink, &QtImageSink::requestsRejected, this, &MainWindow::logRejections, Qt::QueuedConnection);
    connect(m_thumbnails, &ThumbnailPanel::imageSelected, this, &MainWindow::showImage);
    connect(m_thumbnails, &ThumbnailPanel::actionRequested, this,
            [this](viewer_core::ImageId id, ThumbnailAction action) {
                switch (action) {
                case ThumbnailAction::Rename:
                    startRename(id);
                    break;
                case ThumbnailAction::Duplicate:
                    duplicateImage(id);
                    break;
                case ThumbnailAction::Delete:
                    deleteImage(id);
                    break;
                case ThumbnailAction::CopyImage:
                    copyImage(id);
                    break;
                case ThumbnailAction::SaveAs:
                    saveImage(id);
                    break;
                }
            });
    connect(m_infoBar, &ImageInfoBar::renameRequested, this, &MainWindow::renameImage);
    connect(m_imageView, &ImageView::pixelHovered, this, &MainWindow::showPixel);
    connect(m_imageView, &ImageView::hoverLeft, m_pixelLabel, &QLabel::clear);
    updateEditActions();

    switch (dataProblem) {
    case DataFolder::Problem::None:
        break;
    case DataFolder::Problem::NoFolderName: // a wildcard port; any other such endpoint will not start either
        log(LogKind::Warning, tr("Not keeping images or the log: the port changes on every start"));
        break;
    case DataFolder::Problem::Locked:
        log(LogKind::Warning,
            tr("Another viewer uses this endpoint's data folder: keeping the log and images in memory only"));
        break;
    case DataFolder::Problem::Unavailable:
        log(LogKind::Warning,
            tr("Cannot use the data folder in [%1]: keeping the log and images in memory only")
                .arg(QDir::toNativeSeparators(dataRoot)));
        break;
    }

    // Listen only once the event loop runs: images accepted earlier would wait for a UI that is not yet
    // taking them, and a sender's first burst would find the viewer busy.
    setServerState(ServerState::Starting, tr("Starting…"));
    QTimer::singleShot(0, this, &MainWindow::startUp);
}

// Listening comes first, so senders are never refused while up to 2 GiB of stored images load.
void MainWindow::startUp() {
    startServer();
    m_storage->restore([this](const StoredImage& image) { return adoptRestored(image); });
}

// Also Start and Try again. A started server is never paused.
void MainWindow::startServer() {
    m_server.setPaused(false);
    if (const auto error = m_server.start()) {
        setServerState(ServerState::NotListening,
                       tr("Not listening: %1").arg(QString::fromStdString(error->reason)));
        return;
    }
    setServerState(ServerState::Listening, listeningText());
}

// Pause keeps the port and answers every sender "viewer paused", so senders get Rejected.
void MainWindow::togglePause() {
    switch (m_serverControls->state()) {
    case ServerState::Listening:
        m_server.setPaused(true);
        setServerState(ServerState::Paused, tr("Paused: senders get [viewer paused]"));
        break;
    case ServerState::Paused:
        m_server.setPaused(false);
        setServerState(ServerState::Listening, listeningText());
        break;
    case ServerState::Starting:
    case ServerState::Stopped:
    case ServerState::NotListening:
        break;
    }
}

// Stop releases the port, so senders get ViewerUnavailable; another viewer could take it meanwhile.
void MainWindow::toggleRunning() {
    switch (m_serverControls->state()) {
    case ServerState::Listening:
    case ServerState::Paused: {
        const QString endpoint = QString::fromStdString(m_server.boundEndpoint());
        m_server.stop();
        setServerState(ServerState::Stopped, tr("Stopped: released [%1]").arg(endpoint));
        break;
    }
    case ServerState::Stopped:
    case ServerState::NotListening:
        startServer();
        break;
    case ServerState::Starting:
        break;
    }
}

// The Server menu's titles follow the state, as the buttons do.
void MainWindow::setServerState(ServerState state, const QString& text) {
    const bool running = state == ServerState::Listening || state == ServerState::Paused;
    m_serverControls->setState(state, text);
    m_ui->actionPauseServer->setText(state == ServerState::Paused ? tr("Resume") : tr("Pause"));
    m_ui->actionPauseServer->setEnabled(running);
    m_ui->actionStopServer->setText(running || state == ServerState::Starting ? tr("Stop") : tr("Start"));
    m_ui->actionStopServer->setEnabled(state != ServerState::Starting);
    if (state != ServerState::Starting) {
        log(state == ServerState::NotListening ? LogKind::Error : LogKind::Info, text);
    }
}

QString MainWindow::listeningText() const {
    const QString endpoint = QString::fromStdString(m_server.boundEndpoint());
    return m_allowRemote ? tr("Listening on [%1] (remote connections allowed)").arg(endpoint)
                         : tr("Listening on [%1]").arg(endpoint);
}

// The sink counts rejections while the UI is busy, so a flood becomes one entry per reason. Reasons are fixed
// ASCII texts of the server's, never the sender's bytes.
void MainWindow::logRejections() {
    for (const auto& [reason, count] : m_sink.takeRejections()) {
        const QString text = QString::fromStdString(reason);
        log(LogKind::Warning, count == 1 ? tr("Rejected a request: [%1]").arg(text)
                                         : tr("Rejected %1 requests: [%2]").arg(count).arg(text));
    }
}

void MainWindow::log(LogKind kind, const QString& text) {
    m_log->append({std::chrono::system_clock::now(), kind, text});
}

void MainWindow::showLatestEntry() {
    const LogEntry* latest = m_log->latest();
    if (latest == nullptr) {
        m_latestLabel->clear();
        return;
    }
    m_latestLabel->setText(clockTime(toEpochMicroseconds(latest->time)) + u' ' + latest->text);
}

// Closed, the status bar shows the newest entry instead.
void MainWindow::setLogOpen(bool open) {
    {
        const QSignalBlocker blocker(m_logToggle); // this is what its toggled signal calls
        m_logToggle->setChecked(open);
    }
    m_ui->actionShowLog->setChecked(open); // emits toggled, not the triggered it is connected by
    m_logToggle->setIcon(qt_common::makeOutlineIcon(open ? qt_common::OutlineIcon::ChevronDown
                                                         : qt_common::OutlineIcon::ChevronUp));
    m_logPanel->setVisible(open);
    m_latestLabel->setVisible(!open);
    if (open) {
        // Before the window is laid out the splitter has no height: the placeholder then takes what the
        // log leaves, as the stretch factors say.
        const int height = QSettings().value(kLogHeightKey, kDefaultLogHeight).toInt();
        const int above = m_logSplitter->height() > height ? m_logSplitter->height() - height
                                                           : kImageViewSizePlaceholder;
        m_logSplitter->setSizes({above, height});
    }
    QSettings().setValue(kLogOpenKey, open);
}

// Child widgets outlive this window's members, and destroying them moves focus and ends a rename: stop them,
// and the focus signal, reaching into a destroyed history.
MainWindow::~MainWindow() {
    disconnect(qApp, &QApplication::focusChanged, this, nullptr);
    m_thumbnails->disconnect(this);
    m_infoBar->disconnect(this);
    m_imageView->disconnect(this);
    m_storage.reset();
}

void MainWindow::onImageReceived(const visual_debug::ImageMessage& message) {
    addImage(message.name, message.image,
             {.source = viewer_core::ImageSource::Received, .arrivedAt = message.receivedAt});
    m_sink.markHandled();
}

bool MainWindow::addImage(std::string name, cv::Mat image, viewer_core::ImageOrigin origin) {
    viewer_core::ImageHistory::AddResult added;
    try {
        added = m_history.add(std::move(name), std::move(image), origin);
    } catch (const std::exception& error) {
        log(LogKind::Error, tr("Cannot show image: %1").arg(error.what()));
        return false;
    }
    // Timed when it arrived: for a received image, when the server thread finished decoding it.
    const auto& entry = *m_history.find(added.id);
    const QString what = origin.source == viewer_core::ImageSource::OpenedFile ? tr("Opened [%1] (%2)")
                                                                                : tr("Received [%1] (%2)");
    const bool timed = origin.arrivedAt != std::chrono::system_clock::time_point{};
    m_log->append({timed ? origin.arrivedAt : std::chrono::system_clock::now(), LogKind::Info,
                   what.arg(singleLine(QString::fromStdString(entry.name)), imageDetails(entry))});
    // While a rename is open, arrivals do not take the selection, so a burst cannot move the edit. One the
    // search hides arrives quietly: it is listed later, if ever.
    m_thumbnails->addImage(entry, false);
    if (!m_infoBar->isRenaming() && m_thumbnails->isListed(added.id)) {
        m_thumbnails->selectImage(added.id);
    }
    m_storage->store(entry);
    removeEvicted(added.evicted);
    m_storage->sync(m_history.entries());
    updateEditActions();
    return true;
}

std::optional<viewer_core::ImageId> MainWindow::shownImageId() const { return m_currentId; }

void MainWindow::renameImage(viewer_core::ImageId id, QString name) {
    const auto* entry = m_history.find(id);
    if (entry == nullptr) {
        return;
    }
    // The wire format's rule, so a renamed image obeys the same limit as a received one: valid UTF-8, cut
    // to kMaxNameBytes at a character boundary.
    std::string cut = visual_debug::sanitiseName(name.trimmed().toStdString());
    if (cut.empty() || cut == entry->name) {
        return;
    }
    const QString oldName = singleLine(QString::fromStdString(entry->name));
    m_history.rename(id, std::move(cut));
    log(LogKind::Info,
        tr("Renamed [%1] to [%2]").arg(oldName, singleLine(QString::fromStdString(entry->name))));
    m_thumbnails->updateImage(*entry);
    if (m_currentId == id) {
        showImageText(*entry);
    }
    m_storage->sync(m_history.entries());
}

void MainWindow::duplicateImage(viewer_core::ImageId id) {
    const auto duplicated = m_history.duplicate(id);
    if (!duplicated) {
        return;
    }
    log(LogKind::Info,
        tr("Duplicated [%1]").arg(singleLine(QString::fromStdString(m_history.find(id)->name))));
    // Evicted rows go first, so the panel's rows match the history when the copy goes in at its index.
    removeEvicted(duplicated->evicted);
    m_thumbnails->insertImage(*m_history.find(duplicated->id), historyIndexOf(duplicated->id), false);
    selectAndShow(duplicated->id);
    m_storage->store(*m_history.find(duplicated->id));
    m_storage->sync(m_history.entries());
    updateEditActions();
}

void MainWindow::deleteImage(viewer_core::ImageId id) {
    const auto* entry = m_history.find(id);
    if (entry == nullptr) {
        return;
    }
    const QString name = singleLine(QString::fromStdString(entry->name));
    if (!m_history.remove(id)) {
        return;
    }
    log(LogKind::Info, tr("Deleted [%1]").arg(name));
    dropFromView(id);
    m_storage->remove(id);
    m_storage->sync(m_history.entries());
    updateEditActions();
}

void MainWindow::undoDelete() {
    const auto restored = m_history.restoreLastRemoved();
    if (!restored) {
        return;
    }
    log(LogKind::Info,
        tr("Restored [%1]").arg(singleLine(QString::fromStdString(m_history.find(restored->id)->name))));
    removeEvicted(restored->evicted);
    m_thumbnails->insertImage(*m_history.find(restored->id), historyIndexOf(restored->id), false);
    selectAndShow(restored->id);
    m_storage->store(*m_history.find(restored->id)); // its file went when it was deleted
    m_storage->sync(m_history.entries());
    updateEditActions();
}

// The displayed 8-bit picture: other programs cannot take the original's depths.
void MainWindow::copyImage(viewer_core::ImageId id) {
    if (const auto* entry = m_history.find(id)) {
        QGuiApplication::clipboard()->setImage(qt_common::toQImage(entry->display));
        log(LogKind::Info,
            tr("Copied [%1] to the clipboard").arg(singleLine(QString::fromStdString(entry->name))));
    }
}

// Saves what is on screen (the 8-bit display image), not the original: PNG and JPEG cannot hold every
// depth. QImage::save is used because cv::imwrite would swap red and blue in an RGBA image.
void MainWindow::saveImage(viewer_core::ImageId id) {
    const auto* entry = m_history.find(id);
    if (entry == nullptr) {
        return;
    }
    QString suggestedName = QString::fromStdString(entry->name);
    suggestedName.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    suggestedName = QString::fromStdString(viewer_core::saveNameStem(suggestedName.toStdString()));
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Displayed Image"), picturesFolder() + '/' + suggestedName + QStringLiteral(".png"),
        tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".png");
    }
    const QString shownPath = QDir::toNativeSeparators(path);
    if (qt_common::toQImage(entry->display).save(path)) {
        log(LogKind::Info, tr("Saved [%1]").arg(shownPath));
    } else {
        log(LogKind::Error, tr("Could not save [%1]").arg(shownPath));
    }
}

void MainWindow::showImage(viewer_core::ImageId id) {
    const auto* entry = m_history.find(id);
    if (entry == nullptr) {
        return;
    }
    m_currentId = id;
    m_imageView->setImage(qt_common::toQImage(entry->display));
    showImageText(*entry);
    updateEditActions();
}

// Selecting it in the sidebar shows it; one the search hides is shown without a selection.
void MainWindow::selectAndShow(viewer_core::ImageId id) {
    if (!m_thumbnails->selectImage(id)) {
        showImage(id);
    }
}

void MainWindow::showImageText(const viewer_core::ReceivedImage& entry) {
    const QString name = QString::fromStdString(entry.name);
    m_infoBar->showImage(entry.id, name, imageDetails(entry));
    setWindowTitle(viewerWindowTitle(name, m_endpoint));
}

void MainWindow::clearView() {
    m_currentId.reset();
    m_imageView->clear();
    m_infoBar->clear();
    m_pixelLabel->clear();
    setWindowTitle(viewerWindowTitle(QString(), m_endpoint));
    updateEditActions();
}

// The thumbnail pencil selects its image first; the rename itself happens in the info strip.
void MainWindow::startRename(viewer_core::ImageId id) {
    if (m_history.find(id) == nullptr) {
        return;
    }
    if (m_currentId != id) {
        m_thumbnails->selectImage(id);
    }
    if (m_currentId == id) {
        m_infoBar->startRename();
    }
}

// The shared end of Delete and of eviction, once the image has left the history. A shown image is replaced
// before its row goes, so the list never picks the next image by itself.
void MainWindow::dropFromView(viewer_core::ImageId id) {
    if (m_infoBar->renamingId() == id) {
        m_infoBar->cancelRename();
    }
    if (m_currentId == id) {
        if (const auto next = listedNeighbour(id)) {
            m_thumbnails->selectImage(*next); // which shows it
        } else {
            clearView();
        }
    }
    m_thumbnails->removeImage(id);
}

void MainWindow::removeEvicted(const std::vector<viewer_core::ImageId>& evicted) {
    if (evicted.empty()) {
        return;
    }
    for (const auto id : evicted) {
        dropFromView(id);
        m_storage->remove(id);
    }
    log(LogKind::Info, evicted.size() == 1 ? tr("Dropped the oldest image")
                                           : tr("Dropped %1 oldest images").arg(evicted.size()));
}

// The next image down the list as shown, otherwise the one above, skipping rows whose images have already
// left the history (an eviction can take several at once). With the default order, the next older image,
// otherwise the next newer one. For an image the search hides, the first listed image.
std::optional<viewer_core::ImageId> MainWindow::listedNeighbour(viewer_core::ImageId id) const {
    const std::vector<viewer_core::ImageId> listed = m_thumbnails->listedIds();
    const auto inHistory = [this](viewer_core::ImageId candidate) {
        return m_history.find(candidate) != nullptr;
    };
    const auto position = std::find(listed.begin(), listed.end(), id);
    if (position == listed.end()) {
        const auto first = std::find_if(listed.begin(), listed.end(), inHistory);
        return first != listed.end() ? std::optional(*first) : std::nullopt;
    }
    if (const auto below = std::find_if(std::next(position), listed.end(), inHistory);
        below != listed.end()) {
        return *below;
    }
    if (const auto above = std::find_if(std::make_reverse_iterator(position), listed.rend(), inHistory);
        above != listed.rend()) {
        return *above;
    }
    return std::nullopt;
}

int MainWindow::historyIndexOf(viewer_core::ImageId id) const {
    const auto& entries = m_history.entries();
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [id](const viewer_core::ReceivedImage& entry) { return entry.id == id; });
    return static_cast<int>(std::distance(entries.begin(), found));
}

// Edit-menu actions follow focus: while a text field has it, Copy Image, Undo Delete and Delete are off, so
// ⌘C, ⌘Z and ⌘⌫ reach the field and act on its text.
void MainWindow::updateEditActions() {
    // A text field and the log panel keep the copy, undo and delete keys for their own text.
    QWidget* focus = QApplication::focusWidget();
    const bool typing = isTextField(focus) || (focus != nullptr && m_logPanel->isAncestorOf(focus));
    const bool shown = m_currentId.has_value();
    m_ui->actionUndoDelete->setEnabled(m_history.undoDepth() > 0 && !typing);
    m_ui->actionRename->setEnabled(shown);
    m_ui->actionDuplicate->setEnabled(shown);
    m_ui->actionCopyImage->setEnabled(shown && !typing);
    m_ui->actionDelete->setEnabled(shown && !typing);
    m_ui->actionClearHistory->setEnabled(m_history.size() > 0);
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
    const QFileInfo file(path);
    const QString fileName = file.fileName();
    auto result = qt_common::readImageFile(path);
    if (const auto* error = std::get_if<QString>(&result)) {
        log(LogKind::Error, tr("Cannot open [%1]: %2").arg(fileName, *error));
        return;
    }
    addImage(fileName.toStdString(), std::get<cv::Mat>(std::move(result)),
             {.source = viewer_core::ImageSource::OpenedFile,
              .fileBytes = static_cast<std::uint64_t>(file.size()),
              .arrivedAt = std::chrono::system_clock::now()});
}

// --- Keeping images between sessions ----------------------------------------------------------------------

void MainWindow::setConfirmationForTests(std::function<bool(const QString&)> confirm) {
    m_confirm = std::move(confirm);
}

// Restored images go below everything, arrivals during loading included, and only while they fit: arrivals
// take priority for the limits.
std::optional<viewer_core::ImageId> MainWindow::adoptRestored(const StoredImage& image) {
    std::optional<viewer_core::ImageHistory::AddResult> added;
    try {
        added = m_history.addOldest(image.name, image.original, image.origin);
    } catch (const std::exception&) {
        // decode() accepted it, so toDisplayImage can show it; counted as dropped all the same
    }
    if (!added) {
        return std::nullopt;
    }
    m_thumbnails->insertImage(*m_history.find(added->id), historyIndexOf(added->id), false);
    return added->id;
}

void MainWindow::onRestoreFinished(int restored, int skipped, int dropped) {
    if (restored + skipped + dropped > 0) {
        QString text = restored == 1 ? tr("Restored 1 image") : tr("Restored %1 images").arg(restored);
        if (skipped > 0) {
            text += tr("; skipped %1 damaged").arg(skipped);
        }
        if (dropped > 0) {
            text += tr("; dropped %1 that did not fit").arg(dropped);
        }
        log(skipped + dropped > 0 ? LogKind::Warning : LogKind::Info, text);
    }
    if (!m_currentId && !m_infoBar->isRenaming()) {
        if (const std::vector<viewer_core::ImageId> listed = m_thumbnails->listedIds(); !listed.empty()) {
            m_thumbnails->selectImage(listed.front());
        }
    }
    m_storage->sync(m_history.entries());
    updateEditActions();
    emit restoreFinished();
}

// One setting for every viewer. Turning it on writes this viewer's history; turning it off deletes every
// viewer's stored images, after asking.
void MainWindow::setKeepImages(bool keep) {
    if (keep) {
        setKeepImagesSetting(true);
        for (const viewer_core::ReceivedImage& entry : m_history.entries()) {
            m_storage->store(entry);
        }
        m_storage->sync(m_history.entries());
        log(LogKind::Info, tr("Keeping images between sessions"));
        return;
    }
    const int stored = DataFolder::storedImageCountAll(m_dataRoot);
    const QString question =
        stored == 1 ? tr("Delete the stored image?") : tr("Delete the %1 stored images?").arg(stored);
    if (stored > 0 && !confirm(question)) {
        m_ui->actionKeepImages->setChecked(true);
        return;
    }
    // Off before deleting, so that a store already queued finds it off instead of writing after the deletion.
    // Each worker forgets its own images when it next acts.
    setKeepImagesSetting(false);
    DataFolder::removeStoredImagesEverywhere(m_dataRoot, false);
    QString text = tr("Not keeping images between sessions");
    if (stored > 0) {
        text += stored == 1 ? tr(": deleted 1 stored image") : tr(": deleted %1 stored images").arg(stored);
    }
    log(LogKind::Info, text);
}

void MainWindow::clearHistory() {
    const std::size_t count = m_history.size();
    const QString question =
        count == 1 ? tr("Delete the image in the history? This cannot be undone.")
                   : tr("Delete the %1 images in the history? This cannot be undone.").arg(count);
    if (count == 0 || !confirm(question)) {
        return;
    }
    if (m_infoBar->isRenaming()) {
        m_infoBar->cancelRename();
    }
    clearView();
    for (const viewer_core::ReceivedImage& entry : m_history.entries()) {
        m_thumbnails->removeImage(entry.id);
    }
    m_history.clear();
    m_storage->removeAll();
    log(LogKind::Info,
        count == 1 ? tr("Cleared the history: 1 image") : tr("Cleared the history: %1 images").arg(count));
    updateEditActions();
}

bool MainWindow::confirm(const QString& question) {
    if (m_confirm) {
        return m_confirm(question);
    }
    QMessageBox box(QMessageBox::Warning, windowTitle(), question, QMessageBox::Cancel, this);
    QPushButton* remove = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() == remove;
}
