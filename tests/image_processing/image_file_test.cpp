#include "image_file.h"

#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdint>
#include <variant>

namespace {

void writeImage(const QString& path, const cv::Mat& image) {
    ASSERT_TRUE(cv::imwrite(QFile::encodeName(path).toStdString(), image));
}

TEST(ImageFile, KeepsSixteenBitDepth) {
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("deep.png"));
    writeImage(path, cv::Mat(2, 3, CV_16UC1, cv::Scalar(40000)));

    const auto result = qt_common::readImageFile(path);

    ASSERT_TRUE(std::holds_alternative<cv::Mat>(result));
    const cv::Mat& image = std::get<cv::Mat>(result);
    EXPECT_EQ(image.type(), CV_16UC1);
    EXPECT_EQ(image.at<std::uint16_t>(1, 2), 40000);
}

TEST(ImageFile, ReadsNonAsciiPath) {
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    // "č" and "画像" are outside the Western Windows code page, and "画像" outside every European one.
    const QString path = directory.filePath(QString::fromUtf8("môj obrázok č 画像.png"));
    writeImage(path, cv::Mat(2, 2, CV_8UC3, cv::Scalar(1, 2, 3)));

    const auto result = qt_common::readImageFile(path);

    ASSERT_TRUE(std::holds_alternative<cv::Mat>(result));
    EXPECT_EQ(std::get<cv::Mat>(result).type(), CV_8UC3);
}

TEST(ImageFile, ReportsFileThatIsNotAnImage) {
    const QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("notes.png"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("plain text");
    file.close();

    const auto result = qt_common::readImageFile(path);

    ASSERT_TRUE(std::holds_alternative<QString>(result));
    EXPECT_EQ(std::get<QString>(result), QStringLiteral("not a supported image"));
}

TEST(ImageFile, ReportsMissingFile) {
    const auto result = qt_common::readImageFile(QStringLiteral("/nonexistent/missing.png"));

    ASSERT_TRUE(std::holds_alternative<QString>(result));
    EXPECT_EQ(std::get<QString>(result), QStringLiteral("not a supported image"));
}

} // namespace
