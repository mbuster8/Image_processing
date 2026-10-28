#include "image_store.h"

#include "visual_debug/wire_format.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QtEndian>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <string>
#include <tuple>
#include <vector>

namespace {

using std::chrono::microseconds;
using std::chrono::system_clock;

system_clock::time_point timeAt(long long microsecondsSinceEpoch) {
    return system_clock::time_point(
        std::chrono::duration_cast<system_clock::duration>(microseconds(microsecondsSinceEpoch)));
}

cv::Mat smallImage() { return cv::Mat(3, 4, CV_8UC1, cv::Scalar(7)); }

StoredImage indexEntry(const QString& key, std::string name) {
    return {key, std::move(name), {.arrivedAt = timeAt(1'793'000'000'000'001LL)}, {}};
}

QString imagePath(const QTemporaryDir& folder, const QString& key) {
    return QDir(folder.path()).filePath(key + QStringLiteral(".vdimg"));
}

// A valid image beside the store's folder, as "<folder>/../outside.vdimg": a key from the index must never
// reach it.
void writeOutsideImage(const QTemporaryDir& folder) {
    const QString parent = QFileInfo(folder.path()).absolutePath();
    ImageStore beside(parent);
    const QString key = beside.write("outside", smallImage());
    QFile::rename(QDir(parent).filePath(key + QStringLiteral(".vdimg")),
                  QDir(parent).filePath(QStringLiteral("outside.vdimg")));
}

// A file with a fresh key holding `bytes` as they are, for damage the store itself would never write.
QString writeRaw(const QTemporaryDir& folder, const QByteArray& bytes) {
    const QString key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QFile file(imagePath(folder, key));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        return {};
    }
    return key;
}

QByteArray headerLength(quint32 length) {
    QByteArray bytes(4, '\0');
    qToLittleEndian(length, bytes.data());
    return bytes;
}

// Everything the store hands over, without stopping.
struct Loaded {
    std::vector<StoredImage> images;
    ImageStore::LoadResult result;
};

Loaded loadEverything(const ImageStore& store) {
    Loaded loaded;
    loaded.result = store.load([&loaded](StoredImage&& image) {
        loaded.images.push_back(std::move(image));
        return true;
    });
    return loaded;
}

std::string depthLabel(int depth) {
    static const char* const kLabels[] = {"8U", "8S", "16U", "16S", "32S", "32F", "64F"};
    return kLabels[depth];
}

class ImageStoreRoundTrip : public testing::TestWithParam<std::tuple<int, int>> {};

TEST_P(ImageStoreRoundTrip, RoundTripsEveryDepthAndChannelCount) {
    const auto [depth, channels] = GetParam();
    cv::Mat image(5, 7, CV_MAKETYPE(depth, channels));
    cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(100));
    const QTemporaryDir folder;
    ImageStore store(folder.path());

    const QString key = store.write("random", image);
    ASSERT_FALSE(key.isEmpty());
    ASSERT_TRUE(store.writeIndex({indexEntry(key, "random")}));
    const Loaded loaded = loadEverything(store);

    ASSERT_EQ(loaded.images.size(), 1U);
    const cv::Mat& original = loaded.images.front().original;
    ASSERT_EQ(original.type(), image.type());
    ASSERT_EQ(original.size(), image.size());
    EXPECT_EQ(cv::norm(original, image, cv::NORM_INF), 0.0);
}

INSTANTIATE_TEST_SUITE_P(AllDepthsAndChannels, ImageStoreRoundTrip,
                         testing::Combine(testing::Values(CV_8U, CV_8S, CV_16U, CV_16S, CV_32S, CV_32F,
                                                          CV_64F),
                                          testing::Range(1, 5)),
                         [](const testing::TestParamInfo<ImageStoreRoundTrip::ParamType>& info) {
                             return depthLabel(std::get<0>(info.param)) + "C" +
                                    std::to_string(std::get<1>(info.param));
                         });

TEST(ImageStore, KeepsOrderNamesAndOrigin) {
    const QTemporaryDir folder;
    ImageStore store(folder.path());
    const QString older = store.write("older", smallImage());
    const QString newer = store.write("header name", smallImage());
    const viewer_core::ImageOrigin opened{.source = viewer_core::ImageSource::OpenedFile,
                                          .fileBytes = 1234,
                                          .arrivedAt = timeAt(1'793'000'000'123'456LL),
                                          .modifiedAt = timeAt(1'793'000'009'654'321LL)};
    const viewer_core::ImageOrigin received{.arrivedAt = timeAt(1'792'000'000'000'007LL),
                                            .modifiedAt = timeAt(1'792'000'000'000'007LL)};

    ASSERT_TRUE(store.writeIndex({{newer, "renamed", opened, {}}, {older, "older", received, {}}}));
    const Loaded loaded = loadEverything(store);

    ASSERT_EQ(loaded.images.size(), 2U);
    EXPECT_EQ(loaded.result.skipped, 0);
    const StoredImage& first = loaded.images[0];
    EXPECT_EQ(first.key, newer);
    EXPECT_EQ(first.name, "renamed");
    EXPECT_EQ(first.origin.source, viewer_core::ImageSource::OpenedFile);
    EXPECT_EQ(first.origin.fileBytes, std::optional<std::uint64_t>(1234));
    EXPECT_EQ(first.origin.arrivedAt, opened.arrivedAt);
    EXPECT_EQ(first.origin.modifiedAt, opened.modifiedAt);
    const StoredImage& second = loaded.images[1];
    EXPECT_EQ(second.key, older);
    EXPECT_EQ(second.name, "older");
    EXPECT_EQ(second.origin.source, viewer_core::ImageSource::Received);
    EXPECT_EQ(second.origin.fileBytes, std::nullopt);
    EXPECT_EQ(second.origin.arrivedAt, received.arrivedAt);
}

// Images are handed over one at a time, so a caller that has enough reads no further.
TEST(ImageStore, LoadStopsWhenTheCallerHasEnough) {
    const QTemporaryDir folder;
    ImageStore store(folder.path());
    const QString c = store.write("c", smallImage());
    const QString b = store.write("b", smallImage());
    const QString a = store.write("a", smallImage());
    ASSERT_TRUE(store.writeIndex({indexEntry(c, "c"), indexEntry(b, "b"), indexEntry(a, "a")}));
    std::vector<std::string> handed;

    const ImageStore::LoadResult result = store.load([&handed](StoredImage&& image) {
        handed.push_back(image.name);
        return handed.size() < 2;
    });

    EXPECT_EQ(handed, (std::vector<std::string>{"c", "b"}));
    EXPECT_EQ(result.unread, QStringList{a});
    EXPECT_EQ(result.skipped, 0);
}

TEST(ImageStore, SkipsDamagedFiles) {
    const QTemporaryDir root;
    const QTemporaryDir folder(QDir(root.path()).filePath(QStringLiteral("images-XXXXXX")));
    ImageStore store(folder.path());
    writeOutsideImage(folder);
    const QString good = store.write("good", smallImage());
    const QString truncated = store.write("truncated", smallImage());
    ASSERT_TRUE(
        QFile::resize(imagePath(folder, truncated), QFileInfo(imagePath(folder, truncated)).size() - 1));
    const QByteArray magic("VDIMG1\n");
    const QString longHeader =
        writeRaw(folder, magic + headerLength(visual_debug::kMaxHeaderBytes + 1) + QByteArray(16, '{'));
    // Sparse, so it takes no disk space; refused by its size alone.
    const QString huge = writeRaw(folder, magic);
    ASSERT_TRUE(QFile::resize(imagePath(folder, huge),
                              7 + 4 + visual_debug::kMaxHeaderBytes + visual_debug::kMaxPixelBytes + 1));
    const QByteArray tallHeader(R"({"protocol":"visual_debug","version":2,"type":"image","name":"tall",)"
                                R"("rows":999999,"cols":1,"depth":"8U","channels":1})");
    const QString tall = writeRaw(folder, magic + headerLength(tallHeader.size()) + tallHeader);
    const QString missing = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString notAKey = QStringLiteral("../outside");

    ASSERT_TRUE(store.writeIndex({indexEntry(truncated, "truncated"), indexEntry(longHeader, "long header"),
                                  indexEntry(good, "good"), indexEntry(huge, "huge"),
                                  indexEntry(tall, "tall"), indexEntry(missing, "missing"),
                                  indexEntry(notAKey, "not a key"), indexEntry(good, "listed twice")}));
    const Loaded loaded = loadEverything(store);

    ASSERT_EQ(loaded.images.size(), 1U);
    EXPECT_EQ(loaded.images.front().name, "good");
    EXPECT_EQ(loaded.result.skipped, 7);
    EXPECT_FALSE(loaded.result.indexDamaged);
}

TEST(ImageStore, DamagedIndexLoadsNothing) {
    const QTemporaryDir folder;
    ImageStore store(folder.path());
    store.write("kept", smallImage());
    QFile index(QDir(folder.path()).filePath(QStringLiteral("index.json")));
    ASSERT_TRUE(index.open(QIODevice::WriteOnly));
    index.write("{\"version\":1,\"images\":[");
    index.close();

    const Loaded loaded = loadEverything(store);

    EXPECT_TRUE(loaded.images.empty());
    EXPECT_TRUE(loaded.result.indexDamaged);
}

TEST(ImageStore, NoIndexLoadsNothing) {
    const QTemporaryDir folder;

    const Loaded loaded = loadEverything(ImageStore(folder.path()));

    EXPECT_TRUE(loaded.images.empty());
    EXPECT_EQ(loaded.result.skipped, 0);
    EXPECT_FALSE(loaded.result.indexDamaged);
}

TEST(ImageStore, FilesAreOwnerOnly) {
    const QTemporaryDir folder;
    ImageStore store(folder.path());
    const QString key = store.write("private", smallImage());
    ASSERT_TRUE(store.writeIndex({indexEntry(key, "private")}));
    constexpr QFileDevice::Permissions kOthers =
        QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther;

    for (const QString& path :
         {imagePath(folder, key), QDir(folder.path()).filePath(QStringLiteral("index.json"))}) {
        SCOPED_TRACE(path.toStdString());
        EXPECT_TRUE(QFileInfo::exists(path));
        EXPECT_FALSE(QFileInfo(path).permissions() & kOthers);
    }
}

TEST(ImageStore, RemoveDeletesOneImage) {
    const QTemporaryDir root;
    const QTemporaryDir folder(QDir(root.path()).filePath(QStringLiteral("images-XXXXXX")));
    ImageStore store(folder.path());
    writeOutsideImage(folder);
    const QString first = store.write("first", smallImage());
    store.write("second", smallImage());

    EXPECT_TRUE(store.remove(first));

    EXPECT_FALSE(QFileInfo::exists(imagePath(folder, first)));
    EXPECT_EQ(store.storedImageCount(), 1);
    EXPECT_FALSE(store.remove(QStringLiteral("../outside")));
    EXPECT_TRUE(QFileInfo::exists(QDir(root.path()).filePath(QStringLiteral("outside.vdimg"))));
}

TEST(ImageStore, ListsStoredKeys) {
    const QTemporaryDir folder;
    ImageStore store(folder.path());
    const QString first = store.write("first", smallImage());
    const QString second = store.write("second", smallImage());
    ASSERT_TRUE(store.writeIndex({indexEntry(first, "first")}));

    QStringList keys = store.storedKeys();
    keys.sort();
    QStringList expected{first, second};
    expected.sort();

    EXPECT_EQ(keys, expected); // the index lists only one; the files say what is stored
}

TEST(ImageStore, RemoveAllLeavesNothing) {
    const QTemporaryDir folder;
    ImageStore store(folder.path());
    const QString first = store.write("first", smallImage());
    const QString second = store.write("second", smallImage());
    ASSERT_TRUE(store.writeIndex({indexEntry(second, "second"), indexEntry(first, "first")}));
    ASSERT_EQ(store.storedImageCount(), 2);

    store.removeAll();

    EXPECT_EQ(store.storedImageCount(), 0);
    EXPECT_TRUE(QDir(folder.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

} // namespace
