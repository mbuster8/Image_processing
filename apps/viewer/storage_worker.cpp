#include "storage_worker.h"

#include <QLatin1String>
#include <QSet>
#include <QSettings>

#include <utility>

namespace {

const QString kKeepImagesKey = QStringLiteral("viewer/keepImages");

} // namespace

bool keepImagesSetting() {
    QSettings settings;
    settings.sync();
    return settings.value(kKeepImagesKey, false).toBool();
}

void setKeepImagesSetting(bool keep) { QSettings().setValue(kKeepImagesKey, keep); }

StorageWorker::StorageWorker(const QString& imagesFolder, QObject* parent)
    : QObject(parent), m_store(imagesFolder) {
    qRegisterMetaType<StoredImage>(); // loaded crosses to the UI thread by a queued connection
}

void StorageWorker::store(viewer_core::ImageId id, const std::string& name, const cv::Mat& original) {
    if (!keepImagesSetting()) {
        forget();
        return;
    }
    if (m_keys.contains(id)) {
        return;
    }
    const QString key = m_store.write(name, original);
    report(!key.isEmpty(), tr("Cannot store [%1]").arg(QString::fromStdString(name)));
    if (!key.isEmpty()) {
        m_keys.emplace(id, key);
        m_forgotten = false;
    }
}

void StorageWorker::remove(viewer_core::ImageId id) {
    if (!keepImagesSetting()) {
        forget();
        return;
    }
    const auto found = m_keys.find(id);
    if (found == m_keys.end()) {
        return;
    }
    report(m_store.remove(found->second), tr("Cannot delete a stored image"));
    m_keys.erase(found);
}

void StorageWorker::updateIndex(const std::vector<IndexRow>& newestFirst) {
    if (!keepImagesSetting()) {
        forget();
        return;
    }
    std::vector<StoredImage> listed;
    for (const IndexRow& row : newestFirst) {
        if (const auto found = m_keys.find(row.id); found != m_keys.end()) {
            listed.push_back({found->second, row.name, row.origin, {}});
        }
    }
    report(m_store.writeIndex(listed), tr("Cannot write the list of stored images"));
}

void StorageWorker::removeAll() {
    m_store.removeAll();
    m_keys.clear();
}

void StorageWorker::loadAll(std::size_t maxImages, std::size_t maxBytes) {
    if (!keepImagesSetting()) {
        forget();
        emit loadFinished(0, 0);
        return;
    }
    // Arrivals during loading only take room, so what exceeds the empty history's limits could never fit.
    QSet<QString> handed;
    std::size_t bytes = 0;
    int dropped = 0;
    const ImageStore::LoadResult result = m_store.load([&](StoredImage&& image) {
        if (m_stopping) {
            return false;
        }
        const std::size_t imageBytes = viewer_core::entryBytes(image.original);
        if (static_cast<std::size_t>(handed.size()) >= maxImages || bytes + imageBytes > maxBytes) {
            ++dropped;
            return false;
        }
        bytes += imageBytes;
        handed.insert(image.key);
        emit loaded(std::move(image));
        return true;
    });
    if (m_stopping) {
        return; // the window is closing; its index still lists everything not handed over
    }
    if (result.indexDamaged) {
        // Its files stay: a newer viewer's index may only look damaged to this one.
        emit failed(tr("The list of stored images is damaged: none were restored"));
    } else {
        dropped += static_cast<int>(result.unread.size());
        for (const auto& stored : m_keys) {
            handed.insert(stored.second); // written this session, before loading began
        }
        // Crash leftovers, and what did not fit.
        for (const QString& key : m_store.storedKeys()) {
            if (!handed.contains(key)) {
                m_store.remove(key);
            }
        }
    }
    emit loadFinished(result.skipped, dropped);
}

void StorageWorker::keep(viewer_core::ImageId id, const QString& key) {
    if (!keepImagesSetting()) {
        forget();
        return;
    }
    m_keys.emplace(id, key);
    m_forgotten = false;
}

void StorageWorker::discard(const QString& key) { m_store.remove(key); }

void StorageWorker::stopLoading() { m_stopping = true; }

// The setting is off, perhaps since a moment ago in another viewer: this viewer's stored images go too, such
// as one written just before. Once, until something is stored again.
void StorageWorker::forget() {
    if (m_forgotten) {
        return;
    }
    removeAll();
    m_forgotten = true;
}

void StorageWorker::report(bool ok, const QString& what) {
    if (ok) {
        m_failing = false;
    } else if (!std::exchange(m_failing, true)) {
        emit failed(what);
    }
}
