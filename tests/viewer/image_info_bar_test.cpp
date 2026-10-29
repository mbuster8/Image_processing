#include "image_info_bar.h"

#include "viewer_core/display_converter.h"
#include "viewer_core/image_history.h"

#include <QDate>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QTime>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <cstdint>

namespace {

viewer_core::ReceivedImage makeImage(int rows, int cols, int type, viewer_core::ImageOrigin origin = {}) {
    const cv::Mat original(rows, cols, type, cv::Scalar::all(0));
    return {
        1,     "image", original, viewer_core::toDisplayImage(original), viewer_core::describeImage(original),
        origin};
}

QString siSize(std::uint64_t bytes) {
    return QLocale::c().formattedDataSize(static_cast<qint64>(bytes), 1, QLocale::DataSizeSIFormat);
}

// A shown bar with image 7 named "old name".
class ImageInfoBarTest : public testing::Test {
protected:
    void SetUp() override {
        m_bar.showImage(7, QStringLiteral("old name"), QStringLiteral("4×3 · 1 ch · 8U · 12 bytes"));
        m_bar.show();
    }

    QLineEdit* editor() { return m_bar.findChild<QLineEdit*>(QStringLiteral("renameEditor")); }
    QLabel* nameLabel() { return m_bar.findChild<QLabel*>(QStringLiteral("imageName")); }

    ImageInfoBar m_bar;
};

TEST(ImageInfoBar, DetailsForReceivedImage) {
    const auto image = makeImage(480, 640, CV_16SC1);

    EXPECT_EQ(imageDetails(image, QLocale::c()),
              QString::fromUtf8("640×480 · 1 ch · 16S · ") + siSize(614400));
}

TEST(ImageInfoBar, DetailsForOpenedFileUseFileSize) {
    const auto image = makeImage(480, 640, CV_16SC1,
                                 {.source = viewer_core::ImageSource::OpenedFile, .fileBytes = 1'200'000});

    EXPECT_TRUE(imageDetails(image, QLocale::c()).endsWith(siSize(1'200'000) + QStringLiteral(" file")));
}

// The log line and the thumbnails' tool tips show local time to the second, cutting off the rest.
TEST(ImageInfoBar, ClockTimeIsLocalToTheSecond) {
    const QDateTime local(QDate(2026, 10, 27), QTime(14, 5, 9, 999));

    EXPECT_EQ(clockTime(local.toMSecsSinceEpoch() * 1000 + 999), QStringLiteral("14:05:09"));
}

TEST_F(ImageInfoBarTest, EnterEmitsRename) {
    QSignalSpy renamed(&m_bar, &ImageInfoBar::renameRequested);

    m_bar.startRename();
    ASSERT_NE(editor(), nullptr);
    editor()->selectAll();
    QTest::keyClicks(editor(), QStringLiteral("new"));
    QTest::keyClick(editor(), Qt::Key_Return);

    ASSERT_EQ(renamed.size(), 1);
    EXPECT_EQ(renamed.first().at(0).value<quint64>(), 7u);
    EXPECT_EQ(renamed.first().at(1).toString(), QStringLiteral("new"));
    EXPECT_FALSE(m_bar.isRenaming());
}

TEST_F(ImageInfoBarTest, EscapeCancels) {
    QSignalSpy renamed(&m_bar, &ImageInfoBar::renameRequested);

    m_bar.startRename();
    ASSERT_NE(editor(), nullptr);
    editor()->selectAll();
    QTest::keyClicks(editor(), QStringLiteral("new"));
    QTest::keyClick(editor(), Qt::Key_Escape);

    EXPECT_TRUE(renamed.isEmpty());
    EXPECT_FALSE(m_bar.isRenaming());
    ASSERT_NE(nameLabel(), nullptr);
    EXPECT_EQ(nameLabel()->text(), QStringLiteral("old name"));
}

TEST_F(ImageInfoBarTest, UnchangedNameEmitsNothing) {
    QSignalSpy renamed(&m_bar, &ImageInfoBar::renameRequested);

    m_bar.startRename();
    ASSERT_NE(editor(), nullptr);
    QTest::keyClick(editor(), Qt::Key_Return);

    EXPECT_TRUE(renamed.isEmpty());
    EXPECT_FALSE(m_bar.isRenaming());
}

TEST_F(ImageInfoBarTest, RenameIsBoundToTheImageItStartedOn) {
    m_bar.startRename();

    EXPECT_TRUE(m_bar.isRenaming());
    EXPECT_EQ(m_bar.renamingId(), std::optional<viewer_core::ImageId>(7));
}

TEST_F(ImageInfoBarTest, HiddenWithoutImage) {
    m_bar.clear();

    EXPECT_FALSE(m_bar.isVisible());
}

} // namespace
