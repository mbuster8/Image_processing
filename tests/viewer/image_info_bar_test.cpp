#include "image_info_bar.h"

#include "viewer_core/display_converter.h"
#include "viewer_core/image_history.h"
#include "viewer_core/live_channels.h"

#include <QDate>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QTime>
#include <QToolButton>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>
#include <optional>

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

// A snapshot is named after its frame's receive time, local and to the millisecond, so a burst of them stays
// apart.
TEST(ImageInfoBar, SnapshotNameUsesLocalMilliseconds) {
    const QDateTime local(QDate(2026, 10, 30), QTime(14, 3, 22, 517));

    EXPECT_EQ(snapshotName(QStringLiteral("camera"), local.toMSecsSinceEpoch()),
              QStringLiteral("camera 14:03:22.517"));
}

TEST(ImageInfoBar, LiveDetailsShowStateRatesDropsAndAge) {
    using namespace std::chrono_literals;
    const QString summary = QString::fromUtf8("1920×1080 · 3 ch · 8U");
    viewer_core::LiveChannel channel;
    channel.dropped = 12;
    const viewer_core::LiveStats stats{.arrivedPerSecond = 30, .drawnPerSecond = 29, .age = 18ms};

    EXPECT_EQ(liveDetails(summary, channel, stats),
              QString::fromUtf8("live · 1920×1080 · 3 ch · 8U · 30 fps in · 29 drawn · 12 dropped · 18 ms"));
    channel.idle = true;
    EXPECT_EQ(
        liveDetails(summary, channel, {.arrivedPerSecond = 0, .drawnPerSecond = 0, .age = std::nullopt}),
        QString::fromUtf8("idle · 1920×1080 · 3 ch · 8U · 0 fps in · 0 drawn · 12 dropped"));
    channel.frozen = true; // frozen says so even when idle
    EXPECT_TRUE(liveDetails(summary, channel, stats).startsWith(QStringLiteral("frozen · ")));
}

TEST_F(ImageInfoBarTest, LiveChannelHasNoPencil) {
    m_bar.showLiveChannel(QStringLiteral("camera"), QStringLiteral("live · 4×3 · 1 ch · 8U"));

    EXPECT_EQ(nameLabel()->text(), QStringLiteral("camera"));
    EXPECT_FALSE(m_bar.findChild<QToolButton*>(QStringLiteral("renameButton"))->isVisible());
    m_bar.startRename();
    EXPECT_FALSE(m_bar.isRenaming());

    m_bar.showImage(7, QStringLiteral("old name"), QStringLiteral("4×3 · 1 ch · 8U · 12 bytes"));
    EXPECT_TRUE(m_bar.findChild<QToolButton*>(QStringLiteral("renameButton"))->isVisible());
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
