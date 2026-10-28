#include "window_test_support.h"

#include "activity_log.h"
#include "appearance.h"
#include "control_checks.h"
#include "data_folder.h"
#include "display_text.h"
#include "epoch_time.h"
#include "image_info_bar.h"
#include "image_sidebar.h"
#include "image_store.h"
#include "image_view.h"
#include "palette_support.h"
#include "server_controls.h"
#include "shades.h"
#include "storage_worker.h"
#include "thumbnail_model.h"
#include "thumbnail_panel.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPoint>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include "visual_debug/client.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <thread>
#include <vector>

namespace {

using test_support::makeTestWindow;
using viewer_core::ImageId;

cv::Mat makeImage(int rows = 3, int cols = 4) { return cv::Mat(rows, cols, CV_8UC1, cv::Scalar(9)); }

QAction* action(MainWindow& window, const char* name) {
    return window.findChild<QAction*>(QString::fromLatin1(name));
}

ThumbnailPanel* panelOf(MainWindow& window) { return window.findChild<ThumbnailPanel*>(); }

ImageInfoBar* infoBarOf(MainWindow& window) { return window.findChild<ImageInfoBar*>(); }

ImageSidebar* sidebarOf(MainWindow& window) { return window.findChild<ImageSidebar*>(); }

ServerControls* serverControlsOf(MainWindow& window) { return window.findChild<ServerControls*>(); }

ActivityLog* logOf(MainWindow& window) { return window.findChild<ActivityLog*>(); }

// The newest entry whose text contains `text`, or null. Row 0 is the newest.
const LogEntry* findEntry(const ActivityLog& log, const QString& text) {
    for (int row = 0; row < log.rowCount(); ++row) {
        if (log.entryAt(row)->text.contains(text)) {
            return log.entryAt(row);
        }
    }
    return nullptr;
}

int countEntries(const ActivityLog& log, const QString& text) {
    int count = 0;
    for (int row = 0; row < log.rowCount(); ++row) {
        count += log.entryAt(row)->text.contains(text) ? 1 : 0;
    }
    return count;
}

// The test windows listen on an ephemeral port: the endpoint is read from "Listening on [<endpoint>]".
QString listeningEndpoint(MainWindow& window) {
    ServerControls* controls = serverControlsOf(window);
    const bool listening =
        QTest::qWaitFor([controls] { return controls->state() == ServerState::Listening; }, 2000);
    const QString text = controls->text();
    return listening && text.contains(QLatin1Char('['))
               ? text.section(QLatin1Char('['), 1, 1).section(QLatin1Char(']'), 0, 0)
               : QString();
}

visual_debug::SendResult sendTo(const QString& endpoint) {
    visual_debug::Client client(
        {.endpoint = endpoint.toStdString(), .replyTimeout = std::chrono::milliseconds(300)});
    return client.send(makeImage(), "sent");
}

// The name the sidebar lists for the id, or a null string when it is not listed.
QString listedName(MainWindow& window, ImageId id) {
    const QAbstractItemModel* model = panelOf(window)->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(ThumbnailModel::IdRole).value<quint64>() == id) {
            return index.data(ThumbnailModel::NameRole).toString();
        }
    }
    return {};
}

// Adds a, b, c (ids 1, 2, 3); the sidebar lists c, b, a.
void addThree(MainWindow& window) {
    window.addImage("a", makeImage());
    window.addImage("b", makeImage());
    window.addImage("c", makeImage());
}

// The walkthrough found an empty strip between the thumbnails and the image: the sidebar must fill its
// splitter section exactly, with the image area directly beside it and the image view filling its width.
TEST(MainWindowTest, SidebarFillsItsSplitterSection) {
    const auto window = makeTestWindow();
    window->resize(1000, 700);
    window->show();
    QApplication::processEvents();

    const auto* splitter = window->findChild<QSplitter*>(QStringLiteral("mainSplitter"));
    const auto* sidebar = window->findChild<ImageSidebar*>();
    const auto* panel = window->findChild<ThumbnailPanel*>();
    const auto* view = window->findChild<ImageView*>();
    ASSERT_NE(splitter, nullptr);
    ASSERT_NE(sidebar, nullptr);
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(view, nullptr);

    EXPECT_EQ(splitter->widget(0), sidebar);
    EXPECT_EQ(sidebar->width(), splitter->sizes()[0]);
    EXPECT_EQ(panel->width(), sidebar->width());
    EXPECT_TRUE(splitter->widget(1)->isAncestorOf(view));
    EXPECT_EQ(view->width(), splitter->widget(1)->width());
}

TEST(MainWindowTest, WindowTitleNamesImageAndEndpoint) {
    EXPECT_EQ(viewerWindowTitle(QStringLiteral("Sobel X"), QStringLiteral("tcp://127.0.0.1:7139")),
              QString::fromUtf8("Image Processing — Sobel X — 127.0.0.1:7139"));
    EXPECT_EQ(viewerWindowTitle(QString(), QStringLiteral("tcp://127.0.0.1:7140")),
              QString::fromUtf8("Image Processing — 127.0.0.1:7140"));
}

TEST(MainWindowTest, TitleShowsTheEndpointBeforeAnyImage) {
    const auto window = makeTestWindow(QStringLiteral("tcp://127.0.0.1:7141"));

    EXPECT_EQ(window->windowTitle(), QString::fromUtf8("Image Processing — 127.0.0.1:7141"));
}

TEST(MainWindowTest, PixelReadoutHasALeftMargin) {
    const auto window = makeTestWindow();

    const auto* readout = window->findChild<QLabel*>(QStringLiteral("pixelReadout"));

    ASSERT_NE(readout, nullptr);
    EXPECT_GT(readout->contentsMargins().left(), 0);
}

TEST(MainWindowTest, DeleteSelectsNextOlder) {
    const auto window = makeTestWindow();
    addThree(*window);
    ASSERT_TRUE(panelOf(*window)->selectImage(2));

    action(*window, "actionDelete")->trigger();

    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(1));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::optional<ImageId>(1));
    EXPECT_EQ(panelOf(*window)->listedIds(), (std::vector<ImageId>{3, 1}));
}

TEST(MainWindowTest, DeleteOfTheOldestSelectsTheNextNewer) {
    const auto window = makeTestWindow();
    addThree(*window);
    ASSERT_TRUE(panelOf(*window)->selectImage(1));

    action(*window, "actionDelete")->trigger();

    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(2));
}

TEST(MainWindowTest, UndoDeleteRestoresAtItsPlace) {
    const auto window = makeTestWindow();
    addThree(*window);
    ASSERT_TRUE(panelOf(*window)->selectImage(2));
    action(*window, "actionDelete")->trigger();

    action(*window, "actionUndoDelete")->trigger();

    EXPECT_EQ(panelOf(*window)->listedIds(), (std::vector<ImageId>{3, 2, 1}));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::optional<ImageId>(2));
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(2));
}

TEST(MainWindowTest, DeletingTheLastImageClearsEverything) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage());
    auto* readout = window->findChild<QLabel*>(QStringLiteral("pixelReadout"));
    readout->setText(QStringLiteral("X=0 Y=0 · I=9"));

    action(*window, "actionDelete")->trigger();

    EXPECT_TRUE(infoBarOf(*window)->isHidden());
    EXPECT_EQ(window->windowTitle(), viewerWindowTitle(QString(), QStringLiteral("tcp://127.0.0.1:*")));
    EXPECT_TRUE(readout->text().isEmpty());
    EXPECT_FALSE(window->findChild<ImageView*>()->hasImage());
    EXPECT_EQ(window->shownImageId(), std::nullopt);
    EXPECT_TRUE(panelOf(*window)->listedIds().empty());
}

TEST(MainWindowTest, DuplicateAddsCopyAboveAndSelectsIt) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage());

    action(*window, "actionDuplicate")->trigger();

    const std::vector<ImageId> listed = panelOf(*window)->listedIds();
    ASSERT_EQ(listed.size(), 2u);
    EXPECT_EQ(listedName(*window, listed[0]), QStringLiteral("a copy"));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::optional<ImageId>(listed[0]));
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(listed[0]));
}

TEST(MainWindowTest, CopyImagePutsDisplayOnClipboard) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage(6, 8));

    action(*window, "actionCopyImage")->trigger();

    EXPECT_EQ(QGuiApplication::clipboard()->image().size(), QSize(8, 6));
}

TEST(MainWindowTest, RenameUpdatesCaptionTitleAndInfo) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage());

    window->renameImage(1, QStringLiteral("  new  "));

    EXPECT_EQ(listedName(*window, 1), QStringLiteral("new"));
    EXPECT_TRUE(window->windowTitle().contains(QStringLiteral("new")));
    EXPECT_EQ(infoBarOf(*window)->findChild<QLabel*>(QStringLiteral("imageName"))->text(),
              QStringLiteral("new"));
}

TEST(MainWindowTest, RenameIgnoresEmptyName) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage());

    window->renameImage(1, QStringLiteral("   "));

    EXPECT_EQ(listedName(*window, 1), QStringLiteral("a"));
}

TEST(MainWindowTest, RenameCutsLongNameAtCharacterBoundary) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage());

    window->renameImage(1, QString(1023, u'a') + QString::fromUtf8("é"));

    EXPECT_EQ(listedName(*window, 1), QString(1023, u'a'));
}

// The wire format's limit, so that a stored copy reloads with its " copy" instead of its original's name.
TEST(MainWindowTest, DuplicateOfLongNameStaysWithinTheNameLimit) {
    const auto window = makeTestWindow();
    window->addImage(std::string(1024, 'a'), makeImage());

    window->duplicateImage(1);

    EXPECT_EQ(listedName(*window, 2), QString(1019, u'a') + QStringLiteral(" copy"));
}

TEST(MainWindowTest, ArrivalsDuringRenameDoNotMoveTheEdit) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage());
    action(*window, "actionRename")->trigger();

    window->addImage("b", makeImage());
    window->addImage("c", makeImage());
    window->addImage("d", makeImage());

    EXPECT_EQ(infoBarOf(*window)->renamingId(), std::optional<ImageId>(1));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::optional<ImageId>(1));
    auto* editor = window->findChild<QLineEdit*>(QStringLiteral("renameEditor"));
    ASSERT_NE(editor, nullptr);
    editor->setText(QStringLiteral("x"));
    QTest::keyClick(editor, Qt::Key_Return);
    EXPECT_EQ(listedName(*window, 1), QStringLiteral("x"));
    EXPECT_EQ(listedName(*window, 4), QStringLiteral("d"));
}

// The keyboard starts on the thumbnails: the arrow keys step through the images, and the search field shows
// no focus ring in the accent colour until it is used (spec H2).
TEST(MainWindowTest, OpensWithTheThumbnailsFocused) {
    const auto window = makeTestWindow();
    window->show();
    window->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(window.get()));

    EXPECT_EQ(QApplication::focusWidget(), panelOf(*window));
}

// Without a click first, Down steps to the next older image and shows it.
TEST(MainWindowTest, ArrowKeysStepThroughTheImagesFromTheStart) {
    const auto window = makeTestWindow();
    window->show();
    window->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(window.get()));
    addThree(*window);
    ASSERT_TRUE(panelOf(*window)->selectImage(3));

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);

    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(2));
}

TEST(MainWindowTest, EditActionsFollowFocus) {
    const auto window = makeTestWindow();
    window->show();
    window->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(window.get()));
    window->addImage("a", makeImage(6, 8));
    panelOf(*window)->setFocus();
    QGuiApplication::clipboard()->clear();

    // From the sidebar, the shortcut copies the image.
    QTest::keyClick(panelOf(*window), Qt::Key_C, Qt::ControlModifier);
    EXPECT_EQ(QGuiApplication::clipboard()->image().size(), QSize(8, 6));

    // From a text field, the shortcuts act on its text.
    QGuiApplication::clipboard()->clear();
    action(*window, "actionRename")->trigger();
    auto* editor = window->findChild<QLineEdit*>(QStringLiteral("renameEditor"));
    ASSERT_NE(editor, nullptr);
    ASSERT_TRUE(editor->hasFocus());
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
    QTest::keyClick(editor, Qt::Key_Backspace, Qt::ControlModifier);
    EXPECT_NE(QGuiApplication::clipboard()->image().size(), QSize(8, 6));
    EXPECT_EQ(panelOf(*window)->listedIds().size(), 1u);
}

// The log panel takes ⌘C, ⌘Z and ⌘⌫ as a text field does: they never reach the image.
TEST(MainWindowTest, LogPanelKeepsTheEditKeys) {
    const auto window = makeTestWindow();
    window->show();
    window->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(window.get()));
    window->addImage("kept", makeImage(6, 8));
    window->addImage("deleted", makeImage(6, 8));
    panelOf(*window)->setFocus();
    action(*window, "actionDelete")->trigger();
    ASSERT_EQ(panelOf(*window)->listedIds().size(), 1u);
    window->findChild<QToolButton*>(QStringLiteral("logToggle"))->click();
    auto* logList = window->findChild<QListView*>(QStringLiteral("logList"));
    ASSERT_NE(logList, nullptr);
    logList->setFocus();
    ASSERT_TRUE(logList->hasFocus());
    EXPECT_FALSE(action(*window, "actionUndoDelete")->isEnabled());
    QGuiApplication::clipboard()->clear();

    // With nothing selected, every entry, as the panel's Copy button does.
    QTest::keyClick(logList, Qt::Key_C, Qt::ControlModifier);
    EXPECT_TRUE(QGuiApplication::clipboard()->image().isNull());
    EXPECT_EQ(QGuiApplication::clipboard()->text(), logOf(*window)->toPlainText({}));

    QTest::keyClick(logList, Qt::Key_Z, Qt::ControlModifier);
    EXPECT_EQ(panelOf(*window)->listedIds().size(), 1u);
    QTest::keyClick(logList, Qt::Key_Backspace, Qt::ControlModifier);
    EXPECT_EQ(panelOf(*window)->listedIds().size(), 1u);
}

TEST(MainWindowTest, EvictingTheRenamedImageCancelsTheRename) {
    const auto window = makeTestWindow();
    window->addImage("renamed", makeImage());
    action(*window, "actionRename")->trigger();

    for (int i = 0; i < 100; ++i) {
        window->addImage("filler", makeImage());
    }

    EXPECT_FALSE(infoBarOf(*window)->isRenaming());
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(2)); // directly above it, now the oldest
}

TEST(MainWindowTest, HiddenArrivalsDoNotTakeTheView) {
    const auto window = makeTestWindow();
    sidebarOf(*window)->searchField()->setText(QStringLiteral("canny"));

    window->addImage("Canny 1", makeImage());
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(1));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::optional<ImageId>(1));

    window->addImage("Sobel", makeImage());
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(1));
    EXPECT_EQ(sidebarOf(*window)->countLabel()->text(), QStringLiteral("1 of 2"));

    window->addImage("Canny 2", makeImage());
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(3));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::optional<ImageId>(3));
}

TEST(MainWindowTest, SearchKeepsTheShownImage) {
    const auto window = makeTestWindow();
    window->addImage("a", makeImage(3, 4));
    window->addImage("b", makeImage(6, 8));
    ASSERT_TRUE(panelOf(*window)->selectImage(2));

    sidebarOf(*window)->searchField()->setText(QStringLiteral("a"));

    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(2));
    EXPECT_EQ(infoBarOf(*window)->findChild<QLabel*>(QStringLiteral("imageName"))->text(),
              QStringLiteral("b"));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::nullopt);
    action(*window, "actionCopyImage")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->image().size(), QSize(8, 6));
}

TEST(MainWindowTest, DeleteUnderSortSelectsNextListed) {
    const auto window = makeTestWindow();
    window->addImage("c", makeImage());
    window->addImage("a", makeImage());
    window->addImage("b", makeImage());
    sidebarOf(*window)->sortAction(SidebarSort::Name)->trigger(); // a, b, c
    ASSERT_TRUE(panelOf(*window)->selectImage(3));

    action(*window, "actionDelete")->trigger();
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(1)); // c; by history order it would be a
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::optional<ImageId>(1));

    action(*window, "actionDelete")->trigger();
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(2));
}

TEST(MainWindowTest, DeletingAnUnlistedImageShowsFirstListed) {
    const auto window = makeTestWindow();
    window->addImage("x1", makeImage());
    window->addImage("y", makeImage());
    window->addImage("x2", makeImage());
    ASSERT_TRUE(panelOf(*window)->selectImage(2));
    sidebarOf(*window)->searchField()->setText(QStringLiteral("x"));

    action(*window, "actionDelete")->trigger();
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(3));

    sidebarOf(*window)->searchField()->setText(QStringLiteral("zzz"));
    action(*window, "actionDelete")->trigger();
    EXPECT_EQ(window->shownImageId(), std::nullopt);
    EXPECT_FALSE(window->findChild<ImageView*>()->hasImage());
}

TEST(MainWindowTest, UndoUnderSearchShowsRestoredImage) {
    const auto window = makeTestWindow();
    window->addImage("a1", makeImage());
    window->addImage("a2", makeImage());
    window->addImage("b1", makeImage());
    QLineEdit* search = sidebarOf(*window)->searchField();
    search->setText(QStringLiteral("a"));
    ASSERT_TRUE(panelOf(*window)->selectImage(2));
    action(*window, "actionDelete")->trigger();

    search->setText(QStringLiteral("b"));
    action(*window, "actionUndoDelete")->trigger();

    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(2));
    EXPECT_EQ(panelOf(*window)->currentImageId(), std::nullopt);
    EXPECT_EQ(search->text(), QStringLiteral("b"));
}

TEST(MainWindowTest, FindFocusesSearch) {
    const auto window = makeTestWindow();
    window->show();
    window->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(window.get()));

    action(*window, "actionFind")->trigger();

    EXPECT_TRUE(sidebarOf(*window)->searchField()->hasFocus());
}

TEST(MainWindowTest, StopReleasesThePort) {
    const auto window = makeTestWindow();
    const QString endpoint = listeningEndpoint(*window);
    ASSERT_FALSE(endpoint.isEmpty());

    action(*window, "actionStopServer")->trigger();

    EXPECT_EQ(serverControlsOf(*window)->state(), ServerState::Stopped);
    EXPECT_EQ(serverControlsOf(*window)->text(), QStringLiteral("Stopped: released [%1]").arg(endpoint));
    EXPECT_EQ(action(*window, "actionStopServer")->text(), QStringLiteral("Start"));
    EXPECT_EQ(sendTo(endpoint), visual_debug::SendResult::ViewerUnavailable);
}

// Stopped while paused: a started server takes images again.
TEST(MainWindowTest, StartListensAgain) {
    const auto window = makeTestWindow();
    ASSERT_FALSE(listeningEndpoint(*window).isEmpty());
    action(*window, "actionPauseServer")->trigger();
    action(*window, "actionStopServer")->trigger();

    action(*window, "actionStopServer")->trigger();

    EXPECT_EQ(serverControlsOf(*window)->state(), ServerState::Listening);
    EXPECT_EQ(action(*window, "actionStopServer")->text(), QStringLiteral("Stop"));
    EXPECT_EQ(sendTo(listeningEndpoint(*window)), visual_debug::SendResult::Delivered);
}

TEST(MainWindowTest, PauseRejectsSenders) {
    const auto window = makeTestWindow();
    const QString endpoint = listeningEndpoint(*window);
    ASSERT_FALSE(endpoint.isEmpty());

    action(*window, "actionPauseServer")->trigger();
    EXPECT_EQ(serverControlsOf(*window)->state(), ServerState::Paused);
    EXPECT_EQ(serverControlsOf(*window)->text(), QStringLiteral("Paused: senders get [viewer paused]"));
    EXPECT_EQ(action(*window, "actionPauseServer")->text(), QStringLiteral("Resume"));
    EXPECT_EQ(sendTo(endpoint), visual_debug::SendResult::Rejected);

    action(*window, "actionPauseServer")->trigger();
    EXPECT_EQ(serverControlsOf(*window)->state(), ServerState::Listening);
    EXPECT_EQ(sendTo(endpoint), visual_debug::SendResult::Delivered);
}

// The window holds its endpoint's data folder, so a second viewer on the same endpoint keeps its data in
// memory. The server is never started here: the event loop does not run.
TEST(MainWindowTest, HoldsItsDataFolder) {
    const QTemporaryDir root;
    const QString endpoint = QStringLiteral("tcp://127.0.0.1:47139");
    {
        const auto window = makeTestWindow(endpoint, root.path());

        EXPECT_TRUE(QFileInfo(QDir(root.path()).filePath(QStringLiteral("tcp_127.0.0.1_47139"))).isDir());
        EXPECT_EQ(DataFolder::open(root.path(), endpoint), nullptr);
    }
    EXPECT_NE(DataFolder::open(root.path(), endpoint), nullptr);
}

TEST(MainWindowTest, LogsReceivedImageWithItsReceivedTime) {
    const auto window = makeTestWindow();
    const QString endpoint = listeningEndpoint(*window);
    ASSERT_FALSE(endpoint.isEmpty());
    visual_debug::Client client({.endpoint = endpoint.toStdString()});
    const auto before = std::chrono::system_clock::now();

    ASSERT_EQ(client.send(cv::Mat(480, 640, CV_8UC3, cv::Scalar(1, 2, 3)), "camera frame"),
              visual_debug::SendResult::Delivered);
    const auto sent = std::chrono::system_clock::now();
    // The UI takes the image only after this pause: a time taken by the UI would come after `sent`.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const ActivityLog* log = logOf(*window);
    const LogEntry* received = nullptr;
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            received = findEntry(*log, QStringLiteral("camera frame"));
            return received != nullptr;
        },
        2000));

    EXPECT_EQ(received->kind, LogKind::Info);
    EXPECT_TRUE(received->text.contains(QStringLiteral("640×480"))) << received->text.toStdString();
    const QString pixelBytes = QLocale().formattedDataSize(640 * 480 * 3, 1, QLocale::DataSizeSIFormat);
    EXPECT_TRUE(received->text.contains(pixelBytes))
        << received->text.toStdString();
    EXPECT_GE(received->time, before);
    EXPECT_LE(received->time, sent);
}

TEST(MainWindowTest, LogsCombinedRejections) {
    const auto window = makeTestWindow();
    const QString endpoint = listeningEndpoint(*window);
    ASSERT_FALSE(endpoint.isEmpty());
    action(*window, "actionPauseServer")->trigger();
    const ActivityLog* log = logOf(*window);

    // The UI runs no events between the sends, as when it falls behind a flood.
    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(sendTo(endpoint), visual_debug::SendResult::Rejected);
    }
    ASSERT_TRUE(
        QTest::qWaitFor([log] { return findEntry(*log, QStringLiteral("Rejected")) != nullptr; }, 2000));
    EXPECT_EQ(countEntries(*log, QStringLiteral("Rejected")), 1);
    EXPECT_EQ(log->latest()->text, QStringLiteral("Rejected 3 requests: [viewer paused]"));
    EXPECT_EQ(log->latest()->kind, LogKind::Warning);

    ASSERT_EQ(sendTo(endpoint), visual_debug::SendResult::Rejected);
    ASSERT_TRUE(QTest::qWaitFor([log] { return countEntries(*log, QStringLiteral("Rejected")) == 2; }, 2000));
    EXPECT_EQ(log->latest()->text, QStringLiteral("Rejected a request: [viewer paused]"));
}

TEST(MainWindowTest, LogsServerStates) {
    const auto window = makeTestWindow();
    const QString endpoint = listeningEndpoint(*window);
    ASSERT_FALSE(endpoint.isEmpty());
    const ActivityLog* log = logOf(*window);
    EXPECT_NE(findEntry(*log, QStringLiteral("Listening on [%1]").arg(endpoint)), nullptr);

    action(*window, "actionPauseServer")->trigger();
    EXPECT_EQ(log->latest()->text, QStringLiteral("Paused: senders get [viewer paused]"));
    action(*window, "actionStopServer")->trigger();
    EXPECT_EQ(log->latest()->text, QStringLiteral("Stopped: released [%1]").arg(endpoint));
}

// A start that fails names the endpoint, in brackets, from the library's reason.
TEST(MainWindowTest, LogsWhyItIsNotListening) {
    const auto window = makeTestWindow();
    const QString endpoint = listeningEndpoint(*window);
    ASSERT_FALSE(endpoint.isEmpty());

    const auto second = makeTestWindow(endpoint);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return serverControlsOf(*second)->state() == ServerState::NotListening; }, 2000));

    EXPECT_TRUE(logOf(*second)->latest()->text.startsWith(
        QStringLiteral("Not listening: could not listen on [%1]: ").arg(endpoint)))
        << logOf(*second)->latest()->text.toStdString();
}

// Names are in brackets, sizes are not.
TEST(MainWindowTest, LogsArrivalsByName) {
    const auto window = makeTestWindow();
    const ActivityLog* log = logOf(*window);

    window->addImage("frame", makeImage());
    EXPECT_TRUE(log->latest()->text.startsWith(QStringLiteral("Received [frame] (")))
        << log->latest()->text.toStdString();
    window->addImage("photo.png", makeImage(),
                     {.source = viewer_core::ImageSource::OpenedFile, .fileBytes = 10});
    EXPECT_TRUE(log->latest()->text.startsWith(QStringLiteral("Opened [photo.png] (")))
        << log->latest()->text.toStdString();
}

TEST(MainWindowTest, LogsEditsAndEvictions) {
    const auto window = makeTestWindow();
    const ActivityLog* log = logOf(*window);
    addThree(*window);

    window->renameImage(3, QStringLiteral("renamed"));
    EXPECT_EQ(log->latest()->text, QStringLiteral("Renamed [c] to [renamed]"));
    window->duplicateImage(3);
    EXPECT_EQ(log->latest()->text, QStringLiteral("Duplicated [renamed]"));
    window->copyImage(3);
    EXPECT_EQ(log->latest()->text, QStringLiteral("Copied [renamed] to the clipboard"));
    window->deleteImage(3);
    EXPECT_EQ(log->latest()->text, QStringLiteral("Deleted [renamed]"));
    window->undoDelete();
    EXPECT_EQ(log->latest()->text, QStringLiteral("Restored [renamed]"));

    // The history keeps 100: three more than that drop the three oldest in turn.
    for (std::size_t i = 0; i < viewer_core::kDefaultHistoryCapacity; ++i) {
        window->addImage("filler", makeImage());
    }
    EXPECT_NE(findEntry(*log, QStringLiteral("Dropped the oldest image")), nullptr);
}

// The Log toggle ends the status bar, after the server's state.
TEST(MainWindowTest, LogToggleIsAtTheFarRight) {
    const auto window = makeTestWindow();
    window->show();
    auto* toggle = window->findChild<QToolButton*>(QStringLiteral("logToggle"));
    auto* status = window->findChild<QLabel*>(QStringLiteral("serverStatus"));
    ASSERT_NE(toggle, nullptr);
    ASSERT_NE(status, nullptr);

    EXPECT_GT(toggle->mapTo(window.get(), QPoint()).x(), status->mapTo(window.get(), QPoint()).x());
}

// The sidebar, the log and the status bar sit on the panel grey, a step off the image area's background, and
// follow a change of palette at once (spec H3).
TEST(MainWindowTest, PanelsUseThePanelGrey) {
    const test_support::PaletteGuard guard;
    const auto window = makeTestWindow();
    window->show();
    const std::vector<QWidget*> panels = {sidebarOf(*window),
                                          window->findChild<QWidget*>(QStringLiteral("activityLogPanel")),
                                          window->statusBar()};

    for (const QPalette& palette : {test_support::paletteWith(QColor(236, 236, 236), Qt::black),
                                    test_support::paletteWith(QColor(30, 30, 30), Qt::white)}) {
        test_support::applyPalette(palette);
        for (const QWidget* panel : panels) {
            EXPECT_TRUE(panel->autoFillBackground()) << panel->objectName().toStdString();
            EXPECT_EQ(panel->palette().color(QPalette::Window), qt_common::panelGrey(palette))
                << panel->objectName().toStdString();
            EXPECT_EQ(panel->palette().color(QPalette::Base), qt_common::panelGrey(palette))
                << panel->objectName().toStdString();
        }
        EXPECT_EQ(window->findChild<ImageView*>()->palette().color(QPalette::Window),
                  palette.color(QPalette::Window));
    }
}

// The splitter handles are about 7 points wide, with a divider line along the middle (spec H3).
TEST(MainWindowTest, SplitterHandlesShow) {
    const auto window = makeTestWindow();
    window->show();
    window->findChild<QToolButton*>(QStringLiteral("logToggle"))->click();
    const QPalette palette = QGuiApplication::palette();

    for (const char* name : {"mainSplitter", "logSplitter"}) {
        auto* splitter = window->findChild<QSplitter*>(QString::fromLatin1(name));
        ASSERT_NE(splitter, nullptr) << name;
        EXPECT_EQ(splitter->handleWidth(), 7) << name;
        const QImage handle = splitter->handle(1)->grab().toImage();
        const bool across = splitter->orientation() == Qt::Horizontal;
        const QPoint middle = across ? QPoint(handle.width() / 2, 2) : QPoint(2, handle.height() / 2);
        const QPoint edge = across ? QPoint(0, 2) : QPoint(2, 0);
        EXPECT_EQ(handle.pixelColor(middle), qt_common::dividerGrey(palette)) << name;
        EXPECT_EQ(handle.pixelColor(edge), qt_common::panelGrey(palette)) << name;
    }
}

// A divider line runs along the top of the status bar (spec H3).
TEST(MainWindowTest, StatusBarHasADividerOnTop) {
    const auto window = makeTestWindow();
    window->show();
    auto* divider = window->findChild<QWidget*>(QStringLiteral("statusBarDivider"));
    ASSERT_NE(divider, nullptr);

    EXPECT_EQ(divider->height(), 1);
    EXPECT_EQ(divider->mapTo(window.get(), QPoint(0, 1)).y(), window->statusBar()->y());
    EXPECT_EQ(divider->grab().toImage().pixelColor(0, 0), qt_common::dividerGrey(QGuiApplication::palette()));
}

// Large enough to see (spec H1), and in the application's regular font even where the platform gives status
// bars and tool buttons a smaller one.
TEST(MainWindowTest, StatusBarIsLargeEnoughToSee) {
    const test_support::SmallerClassFonts smaller;
    const auto window = makeTestWindow();
    window->show();
    window->addImage("a", makeImage());
    auto* toggle = window->findChild<QToolButton*>(QStringLiteral("logToggle"));

    EXPECT_TRUE(test_support::isLargeFramedButton(toggle));
    EXPECT_TRUE(test_support::usesTheRegularFont(toggle));
    EXPECT_TRUE(test_support::usesTheRegularFont(window->findChild<QLabel*>(QStringLiteral("serverStatus"))));
    EXPECT_TRUE(
        test_support::usesTheRegularFont(window->findChild<QLabel*>(QStringLiteral("latestLogEntry"))));
    EXPECT_TRUE(test_support::usesTheRegularFont(window->findChild<QLabel*>(QStringLiteral("pixelReadout"))));
}

TEST(MainWindowTest, LatestEntryShowsWhileCollapsed) {
    const auto window = makeTestWindow();
    window->show();
    auto* latest = window->findChild<QLabel*>(QStringLiteral("latestLogEntry"));
    auto* toggle = window->findChild<QToolButton*>(QStringLiteral("logToggle"));
    auto* panel = window->findChild<QWidget*>(QStringLiteral("activityLogPanel"));
    ASSERT_NE(latest, nullptr);
    ASSERT_NE(toggle, nullptr);
    ASSERT_NE(panel, nullptr);
    EXPECT_FALSE(panel->isVisible());

    window->addImage("newest", makeImage());

    const LogEntry* entry = logOf(*window)->latest();
    EXPECT_TRUE(latest->isVisible());
    EXPECT_EQ(latest->text(), clockTime(toEpochMicroseconds(entry->time)) + u' ' + entry->text);
    EXPECT_TRUE(entry->text.contains(QStringLiteral("newest")));

    toggle->click();
    EXPECT_TRUE(panel->isVisible());
    EXPECT_FALSE(latest->isVisible());
    toggle->click();
    EXPECT_FALSE(panel->isVisible());
    EXPECT_TRUE(latest->isVisible());
}

TEST(MainWindowTest, LogPanelOpenStateIsRemembered) {
    {
        const auto window = makeTestWindow();
        window->findChild<QToolButton*>(QStringLiteral("logToggle"))->click();
    }
    const auto window = makeTestWindow();
    window->show();

    EXPECT_TRUE(window->findChild<QWidget*>(QStringLiteral("activityLogPanel"))->isVisible());
    EXPECT_TRUE(window->findChild<QToolButton*>(QStringLiteral("logToggle"))->isChecked());
}

TEST(MainWindowTest, ShowLogActionTogglesPanel) {
    const auto window = makeTestWindow();
    window->show();
    QAction* showLog = action(*window, "actionShowLog");
    auto* toggle = window->findChild<QToolButton*>(QStringLiteral("logToggle"));
    auto* panel = window->findChild<QWidget*>(QStringLiteral("activityLogPanel"));
    ASSERT_NE(showLog, nullptr);
    EXPECT_TRUE(showLog->isCheckable());
    EXPECT_EQ(showLog->shortcut(), QKeySequence(QStringLiteral("Ctrl+L")));
    EXPECT_FALSE(showLog->isChecked());

    showLog->trigger();
    EXPECT_TRUE(toggle->isChecked());
    EXPECT_TRUE(panel->isVisible());

    toggle->click();
    EXPECT_FALSE(showLog->isChecked());
    EXPECT_FALSE(panel->isVisible());
}

// The titles of a menu bar's or a menu's entries, without their mnemonics; a separator reads as "-".
QStringList entryTitles(const QList<QAction*>& actions) {
    QStringList titles;
    for (const QAction* entry : actions) {
        titles << (entry->isSeparator() ? QStringLiteral("-") : entry->text().remove(u'&'));
    }
    return titles;
}

TEST(MainWindowTest, MenusAreInTheirOrder) {
    const auto window = makeTestWindow();
    EXPECT_EQ(entryTitles(window->menuBar()->actions()),
              (QStringList{QStringLiteral("File"), QStringLiteral("Edit"), QStringLiteral("View"),
                           QStringLiteral("Server"), QStringLiteral("History")}));
    QStringList view{QStringLiteral("Show in Sidebar"), QStringLiteral("Sort By"), QStringLiteral("-"),
                     QStringLiteral("Show Log")};
    if (qt_common::canChooseAppearance()) {
        view << QStringLiteral("-") << QStringLiteral("Appearance");
    }
    EXPECT_EQ(entryTitles(window->findChild<QMenu*>(QStringLiteral("menuView"))->actions()), view);
}

// Not started: the event loop does not run, so the fixed port is never bound.
TEST(MainWindowTest, LogSurvivesRestart) {
    const QTemporaryDir root;
    const QString endpoint = QStringLiteral("tcp://127.0.0.1:47139");
    makeTestWindow(endpoint, root.path())->addImage("kept", makeImage());

    const auto window = makeTestWindow(endpoint, root.path());

    EXPECT_NE(findEntry(*logOf(*window), QStringLiteral("kept")), nullptr);
}

TEST(MainWindowTest, SaysWhenTheLogIsMemoryOnly) {
    const auto wildcard = makeTestWindow();
    EXPECT_EQ(logOf(*wildcard)->latest()->text,
              QStringLiteral("Not keeping images or the log: the port changes on every start"));
    EXPECT_EQ(logOf(*wildcard)->latest()->kind, LogKind::Warning);

    const QTemporaryDir root;
    const QString endpoint = QStringLiteral("tcp://127.0.0.1:47139");
    const auto first = makeTestWindow(endpoint, root.path());
    const auto second = makeTestWindow(endpoint, root.path());
    EXPECT_EQ(logOf(*first)->rowCount(), 0);
    EXPECT_EQ(logOf(*second)->latest()->text,
              QStringLiteral("Another viewer uses this endpoint's data folder: keeping the log and images in "
                             "memory only"));

    // A folder cannot be made under a file.
    QFile blocker(QDir(root.path()).filePath(QStringLiteral("file")));
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    const QString unusable = blocker.fileName() + QStringLiteral("/root");
    const auto third = makeTestWindow(endpoint, unusable);
    EXPECT_EQ(logOf(*third)->latest()->text,
              QStringLiteral("Cannot use the data folder in [%1]: keeping the log and images in memory only")
                  .arg(QDir::toNativeSeparators(unusable)));
}

TEST(MainWindowTest, CopyAndClearLog) {
    const auto window = makeTestWindow();
    window->show();
    window->findChild<QToolButton*>(QStringLiteral("logToggle"))->click();
    addThree(*window);
    ActivityLog* log = logOf(*window);
    auto* list = window->findChild<QListView*>(QStringLiteral("logList"));
    ASSERT_NE(list, nullptr);

    window->findChild<QToolButton*>(QStringLiteral("logCopy"))->click();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), log->toPlainText({}));
    list->selectionModel()->select(log->index(1), QItemSelectionModel::ClearAndSelect);
    window->findChild<QToolButton*>(QStringLiteral("logCopy"))->click();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), log->data(log->index(1), Qt::DisplayRole).toString());

    window->findChild<QToolButton*>(QStringLiteral("logClear"))->click();
    EXPECT_EQ(log->rowCount(), 0);
    EXPECT_TRUE(window->findChild<QLabel*>(QStringLiteral("latestLogEntry"))->text().isEmpty());
}

// --- Keeping images between sessions ----------------------------------------------------------------------

// A fixed port, so that a restarted window finds the same data folder. The window listens on it once its
// event loop runs, but these tests only look at storage, so they pass whether or not that succeeds (another
// process may hold the port).
QString unusedEndpoint() { return QStringLiteral("tcp://127.0.0.1:47141"); }

QString imagesFolderOf(const QString& root, const QString& endpoint = unusedEndpoint()) {
    return QDir(root).filePath(DataFolder::folderNameFor(endpoint) + QStringLiteral("/images"));
}

int storedFiles(const QString& root, const QString& endpoint = unusedEndpoint()) {
    return ImageStore(imagesFolderOf(root, endpoint)).storedImageCount();
}

bool waitForStoredFiles(const QString& root, int count) {
    return QTest::qWaitFor([&] { return storedFiles(root) == count; }, 5000);
}

// A window on the unused endpoint once it has restored what was stored, or null if it never finishes.
std::unique_ptr<MainWindow> makeRestoredWindow(const QString& root) {
    auto window = makeTestWindow(unusedEndpoint(), root);
    QSignalSpy restored(window.get(), &MainWindow::restoreFinished);
    return restored.wait(5000) ? std::move(window) : nullptr;
}

std::vector<QString> listedNames(MainWindow& window) {
    std::vector<QString> names;
    for (const ImageId id : panelOf(window)->listedIds()) {
        names.push_back(listedName(window, id));
    }
    return names;
}

qint64 listedModifiedTime(MainWindow& window, ImageId id) {
    const QAbstractItemModel* model = panelOf(window)->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(ThumbnailModel::IdRole).value<quint64>() == id) {
            return index.data(ThumbnailModel::ModifiedRole).toLongLong();
        }
    }
    return -1;
}

// The 8-bit picture Copy Image puts on the clipboard.
QImage copiedPicture(MainWindow& window, ImageId id) {
    window.copyImage(id);
    return QGuiApplication::clipboard()->image();
}

cv::Mat randomImage() {
    cv::Mat image(6, 8, CV_8UC1);
    cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(255));
    return image;
}

TEST(MainWindowTest, KeptImagesComeBackAfterRestart) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    QImage pictureA;
    QImage pictureB;
    qint64 renamedAt = 0;
    {
        const auto window = makeRestoredWindow(root.path());
        ASSERT_NE(window, nullptr);
        window->addImage("a", randomImage());
        window->addImage("b", randomImage());
        window->renameImage(2, QStringLiteral("bee"));
        pictureA = copiedPicture(*window, 1);
        pictureB = copiedPicture(*window, 2);
        renamedAt = listedModifiedTime(*window, 2);
    }

    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);

    ASSERT_EQ(listedNames(*window), (std::vector<QString>{QStringLiteral("bee"), QStringLiteral("a")}));
    const std::vector<ImageId> ids = panelOf(*window)->listedIds();
    EXPECT_EQ(copiedPicture(*window, ids[0]), pictureB);
    EXPECT_EQ(copiedPicture(*window, ids[1]), pictureA);
    EXPECT_EQ(listedModifiedTime(*window, ids[0]), renamedAt);
    EXPECT_NE(findEntry(*logOf(*window), QStringLiteral("Restored 2 images")), nullptr);
}

TEST(MainWindowTest, ImagesAreNotStoredByDefault) {
    const QTemporaryDir root;
    {
        const auto window = makeRestoredWindow(root.path());
        ASSERT_NE(window, nullptr);
        window->addImage("a", makeImage());
    }

    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);

    EXPECT_TRUE(panelOf(*window)->listedIds().empty());
    EXPECT_EQ(storedFiles(root.path()), 0);
}

TEST(MainWindowTest, ArrivalsDuringLoadStayNewest) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    {
        const auto window = makeRestoredWindow(root.path());
        ASSERT_NE(window, nullptr);
        window->addImage("a", makeImage());
        window->addImage("b", makeImage());
    }
    {
        const auto window = makeTestWindow(unusedEndpoint(), root.path());
        QSignalSpy restored(window.get(), &MainWindow::restoreFinished);
        window->addImage("c", makeImage()); // before the event loop has started loading

        ASSERT_TRUE(restored.wait(5000));
        EXPECT_EQ(listedNames(*window),
                  (std::vector<QString>{QStringLiteral("c"), QStringLiteral("b"), QStringLiteral("a")}));
    }
    // The arrival is stored too, at the top.
    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(listedNames(*window),
              (std::vector<QString>{QStringLiteral("c"), QStringLiteral("b"), QStringLiteral("a")}));
}

TEST(MainWindowTest, TurningOffDeletesEveryEndpointsImages) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    const QString otherEndpoint = QStringLiteral("tcp://127.0.0.1:47142");
    {
        const auto other = DataFolder::open(root.path(), otherEndpoint);
        ASSERT_NE(other, nullptr);
        ImageStore store(other->imagesFolder());
        store.write("b1", makeImage());
        store.write("b2", makeImage());
    }
    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);
    window->addImage("a", makeImage());
    ASSERT_TRUE(waitForStoredFiles(root.path(), 1));
    QString asked;
    window->setConfirmationForTests([&asked](const QString& text) {
        asked = text;
        return true;
    });

    action(*window, "actionKeepImages")->trigger();

    EXPECT_EQ(asked, QStringLiteral("Delete the 3 stored images?"));
    EXPECT_FALSE(action(*window, "actionKeepImages")->isChecked());
    EXPECT_FALSE(keepImagesSetting());
    EXPECT_EQ(storedFiles(root.path(), otherEndpoint), 0);
    EXPECT_TRUE(QTest::qWaitFor(
        [&] {
            return QDir(imagesFolderOf(root.path())).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty();
        },
        5000));
    EXPECT_EQ(window->shownImageId(), std::optional<ImageId>(1)); // the history itself stays
}

TEST(MainWindowTest, CancellingTurnOffKeepsSetting) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);
    window->addImage("a", makeImage());
    ASSERT_TRUE(waitForStoredFiles(root.path(), 1));
    window->setConfirmationForTests([](const QString&) { return false; });

    action(*window, "actionKeepImages")->trigger();

    EXPECT_TRUE(action(*window, "actionKeepImages")->isChecked());
    EXPECT_TRUE(keepImagesSetting());
    QTest::qWait(50);
    EXPECT_EQ(storedFiles(root.path()), 1);
}

TEST(MainWindowTest, TurningOnStoresTheCurrentHistory) {
    const QTemporaryDir root;
    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);
    window->addImage("a", makeImage());
    window->addImage("b", makeImage());

    action(*window, "actionKeepImages")->trigger();

    EXPECT_TRUE(keepImagesSetting());
    EXPECT_TRUE(waitForStoredFiles(root.path(), 2));
}

TEST(MainWindowTest, StartWithSettingOffCleansUnlockedLeftovers) {
    const QTemporaryDir root;
    const QString releasedEndpoint = QStringLiteral("tcp://127.0.0.1:47142");
    {
        const auto released = DataFolder::open(root.path(), releasedEndpoint);
        ASSERT_NE(released, nullptr);
        ImageStore(released->imagesFolder()).write("left over", makeImage());
    }
    const QString heldEndpoint = QStringLiteral("tcp://127.0.0.1:47143");
    const auto held = DataFolder::open(root.path(), heldEndpoint); // another viewer, still running
    ASSERT_NE(held, nullptr);
    ImageStore(held->imagesFolder()).write("in use", makeImage());

    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);

    EXPECT_EQ(storedFiles(root.path(), releasedEndpoint), 0);
    EXPECT_EQ(storedFiles(root.path(), heldEndpoint), 1);
}

TEST(MainWindowTest, DeleteRemovesFileAndUndoRestoresIt) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    {
        const auto window = makeRestoredWindow(root.path());
        ASSERT_NE(window, nullptr);
        window->addImage("a", makeImage());
        ASSERT_TRUE(waitForStoredFiles(root.path(), 1));

        window->deleteImage(1);
        EXPECT_TRUE(waitForStoredFiles(root.path(), 0));
        window->undoDelete();
        EXPECT_TRUE(waitForStoredFiles(root.path(), 1));
    }
    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(listedNames(*window), std::vector<QString>{QStringLiteral("a")});
}

TEST(MainWindowTest, WildcardEndpointStoresNothing) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    const auto window = makeTestWindow(QStringLiteral("tcp://127.0.0.1:*"), root.path());

    window->addImage("a", makeImage());
    QTest::qWait(100);

    EXPECT_TRUE(QDir(root.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
    EXPECT_NE(findEntry(*logOf(*window),
                        QStringLiteral("Not keeping images or the log: the port changes on every start")),
              nullptr);
}

TEST(MainWindowTest, ClearHistoryEmptiesMemoryAndDisk) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);
    window->addImage("a", makeImage());
    window->addImage("b", makeImage());
    window->deleteImage(2);
    ASSERT_TRUE(waitForStoredFiles(root.path(), 1));
    QString asked;
    window->setConfirmationForTests([&asked](const QString& text) {
        asked = text;
        return true;
    });

    action(*window, "actionClearHistory")->trigger();

    EXPECT_EQ(asked, QStringLiteral("Delete the image in the history? This cannot be undone."));
    EXPECT_TRUE(panelOf(*window)->listedIds().empty());
    EXPECT_EQ(window->shownImageId(), std::nullopt);
    EXPECT_FALSE(action(*window, "actionUndoDelete")->isEnabled());
    EXPECT_TRUE(waitForStoredFiles(root.path(), 0));
}

// A clear while stored images are still loading clears those too: they never come back, now or at the next
// start, and the index names no deleted file.
TEST(MainWindowTest, ClearHistoryDuringRestoreStaysCleared) {
    const QTemporaryDir root;
    setKeepImagesSetting(true);
    {
        const auto window = makeRestoredWindow(root.path());
        ASSERT_NE(window, nullptr);
        window->addImage("a", makeImage());
        window->addImage("b", makeImage());
    }
    {
        const auto window = makeTestWindow(unusedEndpoint(), root.path());
        QSignalSpy restored(window.get(), &MainWindow::restoreFinished);
        window->setConfirmationForTests([](const QString&) { return true; });
        window->addImage("c", makeImage());
        QCoreApplication::processEvents(); // starts the loading, whose images arrive after the clear

        action(*window, "actionClearHistory")->trigger();

        ASSERT_TRUE(restored.count() > 0 || restored.wait(5000));
        EXPECT_TRUE(panelOf(*window)->listedIds().empty());
    }
    const auto window = makeRestoredWindow(root.path());
    ASSERT_NE(window, nullptr);
    EXPECT_TRUE(panelOf(*window)->listedIds().empty());
    EXPECT_EQ(findEntry(*logOf(*window), QStringLiteral("damaged")), nullptr);
    EXPECT_EQ(storedFiles(root.path()), 0);
}

} // namespace
