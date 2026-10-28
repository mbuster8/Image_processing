#pragma once

#include "image_store.h"

#include "viewer_core/image_history.h"

#include <QMetaType>
#include <QObject>
#include <QString>

#include <opencv2/core/mat.hpp>

#include <atomic>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// The one setting for every viewer: whether images are kept between sessions. Off by default. Read afresh
// each time, since another viewer may have changed it.
bool keepImagesSetting();
void setKeepImagesSetting(bool keep);

// One history entry as the index lists it, newest first.
struct IndexRow {
    viewer_core::ImageId id;
    std::string name;
    viewer_core::ImageOrigin origin;
};

Q_DECLARE_METATYPE(StoredImage)

// Keeps one viewer's images on disk, on a thread of its own, so neither the UI nor the server ever waits for
// the disk. Every call first re-reads the setting, which another viewer may have turned off; it then deletes
// this viewer's stored images instead of acting. It maps this session's ids to the store's keys. The window
// calls it through queued invocations only, and it never modifies the images it is given.
class StorageWorker : public QObject {
    Q_OBJECT

public:
    explicit StorageWorker(const QString& imagesFolder, QObject* parent = nullptr);

    void store(viewer_core::ImageId id, const std::string& name, const cv::Mat& original);
    void remove(viewer_core::ImageId id);
    void updateIndex(const std::vector<IndexRow>& newestFirst);
    void removeAll(); // Clear History

    // Emits loaded for each stored image, newest first, while the history could still take it, then
    // loadFinished. What would not fit is never read, and is deleted. Files no index lists are left over from
    // a crash and are deleted.
    void loadAll(std::size_t maxImages, std::size_t maxBytes);
    void keep(viewer_core::ImageId id, const QString& key); // a loaded image the history took
    void discard(const QString& key);                       // a loaded image the history had no room for
    // From any thread: ends a running loadAll at the next image, leaving every file for the next start.
    void stopLoading();

signals:
    void loaded(StoredImage image);
    void loadFinished(int skipped, int dropped);
    // Once until something works again, so a full disk cannot flood the log.
    void failed(QString what);

private:
    void forget();
    void report(bool ok, const QString& what);

    ImageStore m_store;
    std::unordered_map<viewer_core::ImageId, QString> m_keys;
    bool m_failing = false;
    bool m_forgotten = false; // nothing of this viewer's is stored since the setting was last seen off
    std::atomic<bool> m_stopping = false;
};
