#include "storage_controller.h"

#include <utility>
#include <vector>

StorageController::StorageController(const QString& imagesFolder, QObject* parent) : QObject(parent) {
    if (imagesFolder.isEmpty()) {
        return;
    }
    m_worker = new StorageWorker(imagesFolder);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &StorageWorker::loaded, this, &StorageController::onLoaded);
    connect(m_worker, &StorageWorker::loadFinished, this, &StorageController::onLoadFinished);
    connect(m_worker, &StorageWorker::failed, this, &StorageController::failed);
    m_thread.start();
    m_restoring = true; // arrivals before the restore starts must not rewrite the index either
}

StorageController::~StorageController() {
    if (m_worker == nullptr) {
        return;
    }
    m_worker->stopLoading();
    QMetaObject::invokeMethod(m_worker, [] {}, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void StorageController::restore(Adopt adopt) {
    if (m_worker == nullptr) {
        emit restoreFinished(0, 0, 0);
        return;
    }
    m_adopt = std::move(adopt);
    // The window's history has the default limits.
    call([](StorageWorker& worker) {
        worker.loadAll(viewer_core::kDefaultHistoryCapacity, viewer_core::kDefaultHistoryByteBudget);
    });
}

namespace {

// Live snapshots stay in memory, even with Keep Images Between Sessions on: a stream should not fill the
// disk.
bool isStored(const viewer_core::ReceivedImage& entry) {
    return entry.origin.source != viewer_core::ImageSource::LiveSnapshot;
}

} // namespace

// The worker gets its own reference to the pixels, which nothing modifies.
void StorageController::store(const viewer_core::ReceivedImage& entry) {
    if (!isStored(entry)) {
        return;
    }
    call([id = entry.id, name = entry.name, original = entry.original](StorageWorker& worker) {
        worker.store(id, name, original);
    });
}

void StorageController::remove(viewer_core::ImageId id) {
    call([id](StorageWorker& worker) { worker.remove(id); });
}

void StorageController::sync(const std::deque<viewer_core::ReceivedImage>& newestFirst) {
    if (m_worker == nullptr || m_restoring) {
        return;
    }
    std::vector<IndexRow> rows;
    for (const viewer_core::ReceivedImage& entry : newestFirst) {
        if (isStored(entry)) {
            rows.push_back({entry.id, entry.name, entry.origin});
        }
    }
    call([rows = std::move(rows)](StorageWorker& worker) { worker.updateIndex(rows); });
}

void StorageController::removeAll() {
    m_discardRestored = m_restoring;
    call([](StorageWorker& worker) { worker.removeAll(); });
}

// One the history has no room for is gone for good; one loaded after a clear is not taken at all.
void StorageController::onLoaded(const StoredImage& image) {
    if (const auto id = m_discardRestored ? std::nullopt : m_adopt(image)) {
        ++m_restored;
        call([id = *id, key = image.key](StorageWorker& worker) { worker.keep(id, key); });
    } else {
        ++m_dropped;
        call([key = image.key](StorageWorker& worker) { worker.discard(key); });
    }
}

// After a clear there is nothing left to report: every stored file has gone.
void StorageController::onLoadFinished(int skipped, int dropped) {
    const bool cleared = std::exchange(m_discardRestored, false);
    m_restoring = false;
    if (cleared) {
        emit restoreFinished(0, 0, 0);
    } else {
        emit restoreFinished(m_restored, skipped, m_dropped + dropped);
    }
}

void StorageController::call(std::function<void(StorageWorker&)> work) {
    if (m_worker != nullptr) {
        QMetaObject::invokeMethod(
            m_worker, [worker = m_worker, work = std::move(work)] { work(*worker); }, Qt::QueuedConnection);
    }
}
