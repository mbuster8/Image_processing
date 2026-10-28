#pragma once

#include "activity_log.h"
#include "data_folder.h"
#include "qt_image_sink.h"
#include "server_controls.h"
#include "storage_controller.h"

#include "viewer_core/image_history.h"
#include "visual_debug/server.h"
#include "visual_debug/wire_format.h"

#include <QMainWindow>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ActivityLogPanel;
class ImageInfoBar;
class ImageSidebar;
class ImageView;
class QLabel;
class QSplitter;
class QToolButton;
class ThumbnailPanel;

namespace Ui {
class MainWindow;
}

// "Image Processing — <name> — <endpoint>", or without the name when it is empty. The endpoint loses its
// "tcp://", and is always there so that several viewers can be told apart in the Dock and the Window menu.
QString viewerWindowTitle(const QString& imageName, const QString& endpoint);

// The viewer: received and opened images go into one history, shown as thumbnails and in a zoomable view,
// with the original value of the pixel under the cursor in the status bar.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // dataRoot holds one data folder per endpoint; tests pass a temporary one.
    MainWindow(visual_debug::ServerOptions serverOptions, const QString& dataRoot, QWidget* parent = nullptr);
    ~MainWindow() override;

    // The one path for received and opened images: into the history, onto the panel, and out with whatever
    // the history evicted. The panel selects it, and so shows it, unless a rename is open or the search hides
    // it. False, with a status message, for an image the viewer cannot show.
    bool addImage(std::string name, cv::Mat image, viewer_core::ImageOrigin origin = {});

    // The image in the view, which the Edit menu and File → Save As act on.
    std::optional<viewer_core::ImageId> shownImageId() const;

    // Each does nothing for an unknown id.
    void renameImage(viewer_core::ImageId id, QString name); // trimmed and cut; an empty name is ignored
    void duplicateImage(viewer_core::ImageId id);
    void deleteImage(viewer_core::ImageId id);
    void undoDelete();
    void copyImage(viewer_core::ImageId id);
    void saveImage(viewer_core::ImageId id);

    // Answers the questions before deleting stored images or the history, instead of a dialog.
    void setConfirmationForTests(std::function<bool(const QString&)> confirm);

signals:
    void restoreFinished(); // the stored images are back, or there were none to restore

private:
    void startUp();
    void startServer();
    void togglePause();   // Pause, and Resume
    void toggleRunning(); // Stop, and Start and Try again
    void setServerState(ServerState state, const QString& text);
    QString listeningText() const;
    void logRejections();
    void log(LogKind kind, const QString& text);
    void showLatestEntry();
    void setLogOpen(bool open);
    void onImageReceived(const visual_debug::ImageMessage& message);
    std::optional<viewer_core::ImageId> adoptRestored(const StoredImage& image);
    void onRestoreFinished(int restored, int skipped, int dropped);
    void setKeepImages(bool keep);
    void clearHistory();
    bool confirm(const QString& question);
    void showImage(viewer_core::ImageId id);
    void selectAndShow(viewer_core::ImageId id);
    void showImageText(const viewer_core::ReceivedImage& entry);
    void clearView();
    void startRename(viewer_core::ImageId id);
    void dropFromView(viewer_core::ImageId id);
    void removeEvicted(const std::vector<viewer_core::ImageId>& evicted);
    std::optional<viewer_core::ImageId> listedNeighbour(viewer_core::ImageId id) const;
    int historyIndexOf(viewer_core::ImageId id) const;
    void updateEditActions();
    void showPixel(int x, int y);
    void openFile();

    std::unique_ptr<Ui::MainWindow> m_ui;
    std::unique_ptr<DataFolder> m_dataFolder; // null: everything stays in memory
    QString m_dataRoot;
    std::function<bool(const QString&)> m_confirm;
    // A copy's name obeys the wire format's limit, as received and renamed names do.
    viewer_core::ImageHistory m_history{viewer_core::kDefaultHistoryCapacity,
                                        viewer_core::kDefaultHistoryByteBudget, visual_debug::kMaxNameBytes};
    std::unique_ptr<StorageController> m_storage; // reset first when closing, while the history still exists
    // Declared before the server, so the server's thread has stopped before the sink is destroyed.
    QtImageSink m_sink;
    visual_debug::Server m_server;
    ImageSidebar* m_sidebar = nullptr;
    ThumbnailPanel* m_thumbnails = nullptr; // the sidebar's
    ImageInfoBar* m_infoBar = nullptr;
    ImageView* m_imageView = nullptr;
    QLabel* m_pixelLabel = nullptr;
    ServerControls* m_serverControls = nullptr;
    ActivityLog* m_log = nullptr; // a child, so that tests can find it
    ActivityLogPanel* m_logPanel = nullptr;
    QSplitter* m_logSplitter = nullptr;
    QToolButton* m_logToggle = nullptr;
    QLabel* m_latestLabel = nullptr; // the newest entry while the panel is closed
    QString m_endpoint; // as configured, for the window title
    bool m_allowRemote;
    std::optional<viewer_core::ImageId> m_currentId;
};
