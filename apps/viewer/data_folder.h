#pragma once

#include <QLockFile>
#include <QString>

#include <memory>

// One viewer's files: <root>/<folder named after the endpoint>, holding the activity log and the stored
// images. A lock file inside keeps it to one viewer at a time, so two viewers never share files.
class DataFolder {
public:
    // "tcp://127.0.0.1:7139" gives "tcp_127.0.0.1_7139": the scheme, "_", then the rest with every
    // character outside [A-Za-z0-9.] as "_". Empty for a wildcard port, whose folder could never be found
    // again, and for text without "://", which is no endpoint.
    static QString folderNameFor(const QString& endpoint);

    // Why open() gave nothing, so the viewer can say why it keeps everything in memory.
    enum class Problem { None, NoFolderName, Locked, Unavailable };

    // Creates the folder and its images folder, readable by the owner only, and takes the lock. Null, with
    // nothing created, for an endpoint without a folder name; null too when another viewer holds the lock or
    // the folders cannot be made private. The viewer then keeps everything in memory.
    static std::unique_ptr<DataFolder> open(const QString& root, const QString& endpoint,
                                            Problem* problem = nullptr);

    // The stored images of every endpoint's folder under `root`, for asking before deleting them all.
    static int storedImageCountAll(const QString& root);
    // Deletes the stored images of every endpoint's folder under `root`; with onlyUnlocked, only of the
    // folders no running viewer holds, which a crash can leave behind.
    static void removeStoredImagesEverywhere(const QString& root, bool onlyUnlocked);

    QString path() const;
    QString logFile() const;      // <path>/activity.log
    QString imagesFolder() const; // <path>/images

private:
    explicit DataFolder(const QString& path);

    QString m_path;
    QLockFile m_lock;
};
