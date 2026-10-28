#include "data_folder.h"

#include "image_store.h"
#include "private_files.h"

#include <QDir>
#include <QFile>
#include <QStringList>

namespace {

const QString kImagesName = QStringLiteral("images");
const QString kLockName = QStringLiteral("viewer.lock");

bool makePrivateFolder(const QString& path) {
    return QDir().mkpath(path) && QFile::setPermissions(path, kOwnerOnlyFolder);
}

// Every endpoint's folder under the root, whichever viewer made it.
QStringList endpointFolders(const QString& root) {
    QStringList folders;
    const QDir rootFolder(root);
    for (const QString& name : rootFolder.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        folders.append(rootFolder.filePath(name));
    }
    return folders;
}

ImageStore storeIn(const QString& endpointFolder) {
    return ImageStore(QDir(endpointFolder).filePath(kImagesName));
}

} // namespace

QString DataFolder::folderNameFor(const QString& endpoint) {
    const qsizetype separator = endpoint.indexOf(QStringLiteral("://"));
    if (separator < 0 || endpoint.endsWith(QStringLiteral(":*"))) {
        return {};
    }
    // The scheme keeps the name from ever being "." or "..", which would point outside the root.
    QString name = endpoint.left(separator) + u'_' + endpoint.mid(separator + 3);
    for (QChar& character : name) {
        const char16_t code = character.unicode();
        const bool kept = (code >= u'a' && code <= u'z') || (code >= u'A' && code <= u'Z') ||
                          (code >= u'0' && code <= u'9') || code == u'.';
        if (!kept) {
            character = u'_';
        }
    }
    return name;
}

std::unique_ptr<DataFolder> DataFolder::open(const QString& root, const QString& endpoint, Problem* problem) {
    const auto fail = [problem](Problem why) {
        if (problem != nullptr) {
            *problem = why;
        }
        return nullptr;
    };
    const QString name = folderNameFor(endpoint);
    if (name.isEmpty()) {
        return fail(Problem::NoFolderName);
    }
    std::unique_ptr<DataFolder> folder(new DataFolder(QDir(root).filePath(name)));
    if (!makePrivateFolder(folder->path()) || !makePrivateFolder(folder->imagesFolder())) {
        return fail(Problem::Unavailable);
    }
    // The viewer holds the lock for as long as it runs, so its age says nothing: only a dead holder frees it.
    folder->m_lock.setStaleLockTime(0);
    if (!folder->m_lock.tryLock(0)) {
        const bool held = folder->m_lock.error() == QLockFile::LockFailedError;
        return fail(held ? Problem::Locked : Problem::Unavailable);
    }
    if (problem != nullptr) {
        *problem = Problem::None;
    }
    return folder;
}

int DataFolder::storedImageCountAll(const QString& root) {
    int count = 0;
    for (const QString& folder : endpointFolders(root)) {
        count += storeIn(folder).storedImageCount();
    }
    return count;
}

void DataFolder::removeStoredImagesEverywhere(const QString& root, bool onlyUnlocked) {
    for (const QString& folder : endpointFolders(root)) {
        if (!onlyUnlocked) {
            storeIn(folder).removeAll();
            continue;
        }
        // A running viewer holds this lock, so its images stay. Taken only while deleting: a viewer starting
        // meanwhile finds the folder locked and keeps its images in memory.
        QLockFile lock(QDir(folder).filePath(kLockName));
        lock.setStaleLockTime(0);
        if (lock.tryLock(0)) {
            storeIn(folder).removeAll();
        }
    }
}

DataFolder::DataFolder(const QString& path) : m_path(path), m_lock(QDir(path).filePath(kLockName)) {}

QString DataFolder::path() const { return m_path; }

QString DataFolder::logFile() const { return QDir(m_path).filePath(QStringLiteral("activity.log")); }

QString DataFolder::imagesFolder() const { return QDir(m_path).filePath(kImagesName); }
