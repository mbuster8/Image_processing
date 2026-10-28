#include "image_store.h"

#include "epoch_time.h"
#include "private_files.h"

#include "visual_debug/wire_format.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QUuid>
#include <QtEndian>

#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

namespace {

constexpr std::string_view kMagic = "VDIMG1\n";
constexpr qint64 kPrefixBytes = static_cast<qint64>(kMagic.size()) + 4; // the magic and the header length
constexpr qint64 kMaxFileBytes = kPrefixBytes + static_cast<qint64>(visual_debug::kMaxHeaderBytes) +
                                 static_cast<qint64>(visual_debug::kMaxPixelBytes);
// A hundred entries take a few tens of kilobytes; anything near this is not an index the viewer wrote.
constexpr qint64 kMaxIndexBytes = 4 * 1024 * 1024;
constexpr int kIndexVersion = 1;
const QString kIndexName = QStringLiteral("index.json");
const QString kImageSuffix = QStringLiteral(".vdimg");

// Keys come from the index, and become file names: only what QUuid writes can never leave the folder.
bool isValidKey(const QString& key) {
    const QUuid uuid = QUuid::fromString(key);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == key;
}

// JSON numbers are doubles: whole ones up to 2^53 are exact, which microseconds since the epoch stay below
// until the year 2255.
std::optional<qint64> wholeNumber(const QJsonValue& value) {
    constexpr double kExactLimit = 9007199254740992.0;
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number != std::floor(number) || std::abs(number) > kExactLimit) {
        return std::nullopt;
    }
    return static_cast<qint64>(number);
}

QJsonValue sourceName(viewer_core::ImageSource source) {
    return source == viewer_core::ImageSource::OpenedFile ? QStringLiteral("file")
                                                          : QStringLiteral("received");
}

std::optional<viewer_core::ImageSource> sourceFromName(const QJsonValue& name) {
    if (name == QStringLiteral("received")) {
        return viewer_core::ImageSource::Received;
    }
    if (name == QStringLiteral("file")) {
        return viewer_core::ImageSource::OpenedFile;
    }
    return std::nullopt;
}

std::optional<viewer_core::ImageOrigin> originFrom(const QJsonObject& entry) {
    const auto source = sourceFromName(entry.value(QStringLiteral("source")));
    const auto arrived = wholeNumber(entry.value(QStringLiteral("arrivedAt")));
    const auto modified = wholeNumber(entry.value(QStringLiteral("modifiedAt")));
    const auto arrivedAt = arrived ? fromEpochMicroseconds(*arrived) : std::nullopt;
    const auto modifiedAt = modified ? fromEpochMicroseconds(*modified) : std::nullopt;
    if (!source || !arrivedAt || !modifiedAt) {
        return std::nullopt;
    }
    viewer_core::ImageOrigin origin{.source = *source, .arrivedAt = *arrivedAt, .modifiedAt = *modifiedAt};
    if (const QJsonValue bytes = entry.value(QStringLiteral("fileBytes")); !bytes.isUndefined()) {
        const auto fileBytes = wholeNumber(bytes);
        if (!fileBytes || *fileBytes < 0) {
            return std::nullopt;
        }
        origin.fileBytes = static_cast<std::uint64_t>(*fileBytes);
    }
    return origin;
}

// The file's size is checked before anything is read, so a damaged length can never make the viewer read
// more than the largest valid image.
std::optional<visual_debug::ImageMessage> readImageFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const qint64 size = file.size();
    if (size < kPrefixBytes || size > kMaxFileBytes) {
        return std::nullopt;
    }
    const QByteArray prefix = file.read(kPrefixBytes);
    if (prefix.size() != kPrefixBytes || !prefix.startsWith(QByteArrayView(kMagic))) {
        return std::nullopt;
    }
    const qint64 headerBytes = qFromLittleEndian<quint32>(prefix.constData() + kMagic.size());
    if (headerBytes > static_cast<qint64>(visual_debug::kMaxHeaderBytes) ||
        headerBytes > size - kPrefixBytes) {
        return std::nullopt;
    }
    const QByteArray header = file.read(headerBytes);
    const QByteArray pixels = file.read(size - kPrefixBytes - headerBytes);
    if (header.size() != headerBytes || pixels.size() != size - kPrefixBytes - headerBytes) {
        return std::nullopt;
    }
    auto decoded = visual_debug::decode(
        std::string_view(header.constData(), static_cast<std::size_t>(header.size())),
        std::as_bytes(std::span(pixels.constData(), static_cast<std::size_t>(pixels.size()))));
    if (auto* message = std::get_if<visual_debug::ImageMessage>(&decoded)) {
        return std::move(*message);
    }
    return std::nullopt;
}

} // namespace

ImageStore::ImageStore(QString imagesFolder) : m_folder(std::move(imagesFolder)) {}

QString ImageStore::write(const std::string& name, const cv::Mat& original) {
    const auto encoded = visual_debug::encode(original, name);
    const auto* message = std::get_if<visual_debug::EncodedMessage>(&encoded);
    if (message == nullptr) {
        return {};
    }
    const QString key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const bool written = writeFileAtomically(imagePath(key), [message](QIODevice& file) {
        char headerLength[4];
        qToLittleEndian(static_cast<quint32>(message->header.size()), headerLength);
        const std::span<const std::byte> pixels = message->pixelBytes();
        file.write(kMagic.data(), static_cast<qint64>(kMagic.size()));
        file.write(headerLength, sizeof headerLength);
        file.write(message->header.data(), static_cast<qint64>(message->header.size()));
        file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<qint64>(pixels.size()));
        return true;
    });
    return written ? key : QString();
}

bool ImageStore::remove(const QString& key) { return isValidKey(key) && QFile::remove(imagePath(key)); }

bool ImageStore::writeIndex(const std::vector<StoredImage>& newestFirst) {
    QJsonArray images;
    for (const StoredImage& image : newestFirst) {
        QJsonObject entry{{QStringLiteral("key"), image.key},
                          {QStringLiteral("name"), QString::fromStdString(image.name)},
                          {QStringLiteral("source"), sourceName(image.origin.source)},
                          {QStringLiteral("arrivedAt"), toEpochMicroseconds(image.origin.arrivedAt)},
                          {QStringLiteral("modifiedAt"), toEpochMicroseconds(image.origin.modifiedAt)}};
        if (image.origin.fileBytes) {
            entry.insert(QStringLiteral("fileBytes"), static_cast<qint64>(*image.origin.fileBytes));
        }
        images.append(entry);
    }
    const QByteArray json = QJsonDocument(QJsonObject{{QStringLiteral("version"), kIndexVersion},
                                                      {QStringLiteral("images"), images}})
                                .toJson(QJsonDocument::Compact);
    return writeFileAtomically(QDir(m_folder).filePath(kIndexName),
                               [&json](QIODevice& file) { return file.write(json) == json.size(); });
}

ImageStore::LoadResult ImageStore::load(const std::function<bool(StoredImage&&)>& take) const {
    LoadResult result;
    QFile file(QDir(m_folder).filePath(kIndexName));
    if (!file.open(QIODevice::ReadOnly)) {
        result.indexDamaged = file.exists(); // no index is simply nothing stored yet
        return result;
    }
    if (file.size() > kMaxIndexBytes) {
        result.indexDamaged = true;
        return result;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = document.object();
    const QJsonValue images = root.value(QStringLiteral("images"));
    if (!document.isObject() || root.value(QStringLiteral("version")) != kIndexVersion || !images.isArray()) {
        result.indexDamaged = true;
        return result;
    }
    // A key listed twice would let one image's removal delete the other's file.
    QSet<QString> seen;
    bool stopped = false;
    for (const QJsonValue& value : images.toArray()) {
        const QJsonObject entry = value.toObject();
        const QString key = entry.value(QStringLiteral("key")).toString();
        const std::optional<viewer_core::ImageOrigin> origin = originFrom(entry);
        if (!isValidKey(key) || seen.contains(key) || !origin) {
            ++result.skipped;
            continue;
        }
        seen.insert(key);
        if (stopped) {
            result.unread.append(key);
            continue;
        }
        std::optional<visual_debug::ImageMessage> message = readImageFile(imagePath(key));
        if (!message) {
            ++result.skipped;
            continue;
        }
        // The shown name obeys the wire format's rule, as a rename does; without one, the header's name.
        std::string name =
            visual_debug::sanitiseName(entry.value(QStringLiteral("name")).toString().toStdString());
        stopped = !take({key, name.empty() ? std::move(message->name) : std::move(name), *origin,
                         std::move(message->image)});
    }
    return result;
}

int ImageStore::storedImageCount() const { return static_cast<int>(storedKeys().size()); }

QStringList ImageStore::storedKeys() const {
    QStringList keys;
    for (const QString& name : QDir(m_folder).entryList({QStringLiteral("*") + kImageSuffix}, QDir::Files)) {
        keys.append(name.chopped(kImageSuffix.size()));
    }
    return keys;
}

void ImageStore::removeAll() {
    QDir folder(m_folder);
    for (const QString& name : folder.entryList(QDir::Files | QDir::Hidden | QDir::System)) {
        folder.remove(name);
    }
}

QString ImageStore::imagePath(const QString& key) const {
    return QDir(m_folder).filePath(key + kImageSuffix);
}
