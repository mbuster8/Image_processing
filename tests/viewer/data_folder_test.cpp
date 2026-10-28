#include "data_folder.h"

#include "image_store.h"

#include <QDir>
#include <QFileDevice>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

namespace {

const QString kEndpoint = QStringLiteral("tcp://127.0.0.1:7139");

TEST(DataFolder, NamesFolderAfterEndpoint) {
    EXPECT_EQ(DataFolder::folderNameFor(kEndpoint), QStringLiteral("tcp_127.0.0.1_7139"));
    // The scheme, "_", then the address with every character outside [A-Za-z0-9.] as "_".
    EXPECT_EQ(DataFolder::folderNameFor(QStringLiteral("tcp://[::1]:7140")),
              QStringLiteral("tcp____1__7140"));
    EXPECT_EQ(DataFolder::folderNameFor(QStringLiteral("tcp://127.0.0.1:*")), QString());
    // Not an endpoint: as a folder name it would point outside the root.
    EXPECT_EQ(DataFolder::folderNameFor(QStringLiteral("..")), QString());
}

TEST(DataFolder, CreatesOwnerOnlyFolders) {
    const QTemporaryDir root;

    const auto folder = DataFolder::open(root.path(), kEndpoint);

    ASSERT_NE(folder, nullptr);
    EXPECT_EQ(folder->path(), QDir(root.path()).filePath(QStringLiteral("tcp_127.0.0.1_7139")));
    EXPECT_EQ(folder->logFile(), folder->path() + QStringLiteral("/activity.log"));
    EXPECT_EQ(folder->imagesFolder(), folder->path() + QStringLiteral("/images"));
    constexpr QFileDevice::Permissions kOthers = QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                                 QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                                 QFileDevice::WriteOther | QFileDevice::ExeOther;
    for (const QString& path : {folder->path(), folder->imagesFolder()}) {
        SCOPED_TRACE(path.toStdString());
        const QFileInfo info(path);
        EXPECT_TRUE(info.isDir());
        EXPECT_FALSE(info.permissions() & kOthers);
    }
}

TEST(DataFolder, SecondOpenFailsWhileLocked) {
    const QTemporaryDir root;
    auto first = DataFolder::open(root.path(), kEndpoint);
    ASSERT_NE(first, nullptr);

    DataFolder::Problem problem = DataFolder::Problem::None;
    EXPECT_EQ(DataFolder::open(root.path(), kEndpoint, &problem), nullptr);
    EXPECT_EQ(problem, DataFolder::Problem::Locked);

    first.reset();
    EXPECT_NE(DataFolder::open(root.path(), kEndpoint), nullptr);
}

TEST(DataFolder, WildcardPortKeepsNothing) {
    const QTemporaryDir root;

    DataFolder::Problem problem = DataFolder::Problem::None;
    EXPECT_EQ(DataFolder::open(root.path(), QStringLiteral("tcp://127.0.0.1:*"), &problem), nullptr);
    EXPECT_EQ(problem, DataFolder::Problem::NoFolderName);
    EXPECT_TRUE(QDir(root.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

// Turning storage off counts and deletes every endpoint's images; starting with it off deletes only those no
// running viewer holds.
TEST(DataFolder, CountsAndRemovesStoredImagesEverywhere) {
    const QTemporaryDir root;
    const auto held = DataFolder::open(root.path(), kEndpoint);
    ASSERT_NE(held, nullptr);
    ImageStore(held->imagesFolder()).write("held", cv::Mat(2, 2, CV_8UC1, cv::Scalar(1)));
    QString releasedImages;
    {
        const auto released = DataFolder::open(root.path(), QStringLiteral("tcp://127.0.0.1:7140"));
        ASSERT_NE(released, nullptr);
        releasedImages = released->imagesFolder();
        ImageStore store(releasedImages);
        store.write("released 1", cv::Mat(2, 2, CV_8UC1, cv::Scalar(1)));
        store.write("released 2", cv::Mat(2, 2, CV_8UC1, cv::Scalar(1)));
    }
    EXPECT_EQ(DataFolder::storedImageCountAll(root.path()), 3);

    DataFolder::removeStoredImagesEverywhere(root.path(), true);
    EXPECT_EQ(ImageStore(held->imagesFolder()).storedImageCount(), 1);
    EXPECT_EQ(ImageStore(releasedImages).storedImageCount(), 0);

    DataFolder::removeStoredImagesEverywhere(root.path(), false);
    EXPECT_EQ(DataFolder::storedImageCountAll(root.path()), 0);
}

} // namespace
