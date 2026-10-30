#pragma once

#include "activity_log.h"
#include "data_folder.h"
#include "qt_image_sink.h"
#include "server_controls.h"
#include "storage_controller.h"

#include "image_view.h"

#include "viewer_core/display_settings.h"
#include "viewer_core/image_history.h"
#include "viewer_core/live_channels.h"
#include "visual_debug/server.h"
#include "visual_debug/wire_format.h"

#include <QImage>
#include <QMainWindow>
#include <QString>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

class ActivityLogPanel;
class ImageInfoBar;
class ImageSidebar;
class QLabel;
class QSplitter;
class QTimer;
class QToolButton;
class ThumbnailPanel;

namespace Ui {
class MainWindow;
}

// "Visual Debug Viewer — <name> — <endpoint>", or without the name when it is empty. The endpoint loses its
// "tcp://", and is always there so that several viewers can be told apart in the Dock and the Window menu.
QString viewerWindowTitle(const QString& imageName, const QString& endpoint);

// The viewer: received and opened images go into one history, shown as thumbnails and in a zoomable view,
// with the original value of the pixel under the cursor in the status bar. Live channels update in place
// beside the history; the view shows either a history image or a channel, never both.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    using Clock = std::chrono::steady_clock;

    // dataRoot holds one data folder per endpoint; tests pass a temporary one.
    MainWindow(visual_debug::ServerOptions serverOptions, const QString& dataRoot, QWidget* parent = nullptr);
    // The clock times live channels: their rates, and when they go idle. Tests pass one they move.
    MainWindow(visual_debug::ServerOptions serverOptions, const QString& dataRoot,
               std::function<Clock::time_point()> clock, QWidget* parent = nullptr);
    ~MainWindow() override;

    // The one path for received and opened images: into the history, onto the panel, and out with whatever
    // the history evicted. The panel selects it, and so shows it, unless a rename is open or the search hides
    // it. False, with a status message, for an image the viewer cannot show.
    bool addImage(std::string name, cv::Mat image, viewer_core::ImageOrigin origin = {});

    // The image in the view, which the Edit menu and File → Save As act on.
    std::optional<viewer_core::ImageId> shownImageId() const;
    std::optional<std::string> shownLiveChannel() const; // the channel in the view instead

    // Every 500 ms while live channels exist: idle states, the rows' pictures and rates, the info strip.
    void tick(Clock::time_point now);
    QtImageSink& sinkForTests();

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
    void logRejections();   // when the sink has new counts: now, unless a line went out in the last second
    void writeRejections(); // takes the counts and logs them, all but the paused ones while paused
    void logPausedRejections();
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
    // The picture the view, Copy Image and Save As show: the stored display picture, or one drawn with the
    // display settings of the entry's name.
    QImage shownPicture(const viewer_core::ReceivedImage& entry) const;
    void changeDisplaySettings(const viewer_core::DisplaySettings& settings);
    void takeLiveFrames();
    void showLiveChannel(const std::string& name);
    void drawLiveFrame(ImageView::KeepView keep); // the shown channel's frame
    void showLiveText(Clock::time_point now);     // the shown channel's info strip
    void updateLiveRow(const viewer_core::LiveChannel& channel, Clock::time_point now);
    void closeLiveChannel(const std::string& name); // the close button
    // Closed or evicted. A shown channel gives the view back to the image shown before it.
    void forgetLiveChannel(const std::string& name);
    void setLiveFrozen(bool frozen);
    void takeSnapshot();
    QImage livePicture() const; // the shown channel's frame as on screen; null without one
    void copyLiveFrame();
    void saveLiveFrame();
    void savePicture(const QImage& picture, const QString& name); // asks where, then saves
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
    std::function<Clock::time_point()> m_clock; // before the sink, which shares it
    std::unique_ptr<DataFolder> m_dataFolder;   // null: everything stays in memory
    QString m_dataRoot;
    std::function<bool(const QString&)> m_confirm;
    // A copy's name obeys the wire format's limit, as received and renamed names do.
    viewer_core::ImageHistory m_history{viewer_core::kDefaultHistoryCapacity,
                                        viewer_core::kDefaultHistoryByteBudget, visual_debug::kMaxNameBytes};
    viewer_core::DisplaySettingsStore m_displaySettings; // by name, for this session
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
    QString m_endpoint;              // as configured, for the window title
    bool m_allowRemote;
    std::optional<viewer_core::ImageId> m_currentId;
    std::optional<viewer_core::ImageId> m_lastHistoryId; // shown again when the channel in its place closes
    viewer_core::LiveChannels m_live;
    std::optional<std::string> m_liveChannel; // shown instead of m_currentId
    // The rows' pictures, by channel, with the frame count each was made at: a picture is made again only
    // for a newer frame.
    std::map<std::string, std::pair<std::uint64_t, QImage>> m_rowPictures;
    std::set<std::string> m_refusedChannels; // logged once each, until the name starts a channel
    QTimer* m_liveTimer = nullptr;
    QTimer* m_rejectionTimer = nullptr; // running for a second after a rejection line
    int m_pausedRejections = 0;
};
