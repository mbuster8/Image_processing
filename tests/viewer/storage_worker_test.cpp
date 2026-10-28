#include "image_store.h"
#include "storage_worker.h"

#include "viewer_core/image_history.h"

#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace {

cv::Mat smallImage() { return cv::Mat(3, 4, CV_8UC1, cv::Scalar(7)); }

// Three stored images, listed newest first: "c", "b", "a".
void storeThree(const QString& folder) {
    ImageStore store(folder);
    std::vector<StoredImage> listed;
    for (const char* name : {"c", "b", "a"}) {
        listed.push_back({store.write(name, smallImage()), name, {}, {}});
    }
    ASSERT_TRUE(store.writeIndex(listed));
}

std::vector<std::string> loadedNames(const QSignalSpy& loaded) {
    std::vector<std::string> names;
    for (const QList<QVariant>& arguments : loaded) {
        names.push_back(arguments.at(0).value<StoredImage>().name);
    }
    return names;
}

int droppedIn(const QSignalSpy& finished) { return finished.at(0).at(1).toInt(); }

// The history could take no more: the rest is never read, counted as dropped, and its files deleted.
TEST(StorageWorker, LoadStopsAtTheHistorysCapacity) {
    const QTemporaryDir folder;
    setKeepImagesSetting(true);
    storeThree(folder.path());
    StorageWorker worker(folder.path());
    QSignalSpy loaded(&worker, &StorageWorker::loaded);
    QSignalSpy finished(&worker, &StorageWorker::loadFinished);

    worker.loadAll(2, viewer_core::kDefaultHistoryByteBudget);

    EXPECT_EQ(loadedNames(loaded), (std::vector<std::string>{"c", "b"}));
    ASSERT_EQ(finished.count(), 1);
    EXPECT_EQ(droppedIn(finished), 1);
    EXPECT_EQ(ImageStore(folder.path()).storedImageCount(), 2);
}

TEST(StorageWorker, LoadStopsAtTheHistorysByteBudget) {
    const QTemporaryDir folder;
    setKeepImagesSetting(true);
    storeThree(folder.path());
    StorageWorker worker(folder.path());
    QSignalSpy loaded(&worker, &StorageWorker::loaded);
    QSignalSpy finished(&worker, &StorageWorker::loadFinished);

    worker.loadAll(viewer_core::kDefaultHistoryCapacity, 2 * viewer_core::entryBytes(smallImage()));

    EXPECT_EQ(loadedNames(loaded), (std::vector<std::string>{"c", "b"}));
    ASSERT_EQ(finished.count(), 1);
    EXPECT_EQ(droppedIn(finished), 1);
    EXPECT_EQ(ImageStore(folder.path()).storedImageCount(), 2);
}

// Closing the window stops a restore: everything not yet handed over stays for the next start.
TEST(StorageWorker, StoppedLoadKeepsEveryFile) {
    const QTemporaryDir folder;
    setKeepImagesSetting(true);
    storeThree(folder.path());
    StorageWorker worker(folder.path());
    QSignalSpy loaded(&worker, &StorageWorker::loaded);
    QSignalSpy finished(&worker, &StorageWorker::loadFinished);

    worker.stopLoading();
    worker.loadAll(viewer_core::kDefaultHistoryCapacity, viewer_core::kDefaultHistoryByteBudget);

    EXPECT_EQ(loaded.count(), 0);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(ImageStore(folder.path()).storedImageCount(), 3);
}

// Another viewer turned the setting off: this viewer's files go, and nothing loads.
TEST(StorageWorker, SettingOffDeletesInsteadOfLoading) {
    const QTemporaryDir folder;
    storeThree(folder.path());
    setKeepImagesSetting(false);
    StorageWorker worker(folder.path());
    QSignalSpy loaded(&worker, &StorageWorker::loaded);
    QSignalSpy finished(&worker, &StorageWorker::loadFinished);

    worker.loadAll(viewer_core::kDefaultHistoryCapacity, viewer_core::kDefaultHistoryByteBudget);

    EXPECT_EQ(loaded.count(), 0);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_EQ(ImageStore(folder.path()).storedImageCount(), 0);
}

} // namespace
