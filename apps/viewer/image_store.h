#pragma once

#include "viewer_core/image_history.h"

#include <QString>
#include <QStringList>

#include <opencv2/core/mat.hpp>

#include <functional>
#include <string>
#include <vector>

// One stored image: its file's key, the name the viewer shows (after any rename), where it came from and
// its times, and the original pixels.
struct StoredImage {
    QString key;
    std::string name;
    viewer_core::ImageOrigin origin;
    cv::Mat original;
};

// The images a viewer keeps between sessions, in its data folder's images folder:
// - one file per image, "<key>.vdimg": "VDIMG1\n", the header's length as a little-endian uint32, the wire
//   format's header, then the pixel bytes, so every depth is kept exactly;
// - "index.json", newest first, with what the header does not hold: the shown name, the source, the times.
// Loading validates every file with visual_debug::decode, as the server does a received image. The store
// only touches files, so it may run on any thread, one at a time.
class ImageStore {
public:
    explicit ImageStore(QString imagesFolder);

    // The new image's key, or empty when it cannot be encoded or written.
    QString write(const std::string& name, const cv::Mat& original);
    bool remove(const QString& key); // false too for a key the store would never make
    // Metadata only (the pixels stay in their files); replaces the index at once.
    bool writeIndex(const std::vector<StoredImage>& newestFirst);

    struct LoadResult {
        int skipped = 0;           // index entries without a valid image
        bool indexDamaged = false; // then nothing loads
        QStringList unread;        // the keys listed after the image at which `take` stopped the load
    };
    // Hands each valid image to `take`, newest first, as soon as it is read, so that the caller never holds
    // more than it keeps. `take` returns false to stop: the images listed after it are not read.
    LoadResult load(const std::function<bool(StoredImage&&)>& take) const;

    int storedImageCount() const;
    QStringList storedKeys() const; // every image file's key, listed in the index or not
    void removeAll();               // every file in the folder, the index included

private:
    QString imagePath(const QString& key) const;

    QString m_folder;
};
