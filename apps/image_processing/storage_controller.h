#pragma once

#include "storage_worker.h"

#include "viewer_core/image_history.h"

#include <QObject>
#include <QString>
#include <QThread>

#include <deque>
#include <functional>
#include <optional>

// Keeps one viewer's history on disk between sessions. It drives a StorageWorker on a thread of its own, so
// neither the UI nor the server waits for the disk, and it lives on the UI thread, where the window tells it
// every change to the history. It never changes the history itself: restoring hands each stored image to
// the window to take. Without an images folder every call does nothing, and a restore finishes at once.
class StorageController : public QObject {
    Q_OBJECT

public:
    // Takes a restored image into the history: its id there, or nullopt when it does not fit.
    using Adopt = std::function<std::optional<viewer_core::ImageId>(const StoredImage&)>;

    explicit StorageController(const QString& imagesFolder, QObject* parent = nullptr);
    // Stops a restore, then waits for the writes already asked for, so closing never loses the newest image.
    ~StorageController() override;

    // Hands the stored images to adopt, newest first, then emits restoreFinished. The index is not rewritten
    // until then, so it cannot lose the images still loading.
    void restore(Adopt adopt);

    void store(const viewer_core::ReceivedImage& entry);
    void remove(viewer_core::ImageId id);
    void sync(const std::deque<viewer_core::ReceivedImage>& newestFirst); // the index
    void removeAll(); // Clear History: the images still to be restored go too

signals:
    void restoreFinished(int restored, int skipped, int dropped);
    void failed(QString what);

private:
    void onLoaded(const StoredImage& image);
    void onLoadFinished(int skipped, int dropped);
    void call(std::function<void(StorageWorker&)> work);

    QThread m_thread;
    StorageWorker* m_worker = nullptr; // on m_thread
    Adopt m_adopt;
    bool m_restoring = false;
    bool m_discardRestored = false; // cleared while restoring
    int m_restored = 0;
    int m_dropped = 0;
};
