#include "thumbnail_panel.h"

#include "viewer_core/display_converter.h"
#include "viewer_core/image_history.h"

#include <QListWidgetItem>
#include <QSignalSpy>
#include <QString>

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <string>
#include <utility>

namespace {

viewer_core::ReceivedImage makeEntry(viewer_core::ImageId id, std::string name) {
    const cv::Mat original(3, 4, CV_8UC1, cv::Scalar(9));
    return {id, std::move(name), original, viewer_core::toDisplayImage(original), viewer_core::describeImage(original)};
}

quint64 idOf(const QListWidgetItem* item) { return item->data(Qt::UserRole).value<quint64>(); }

TEST(ThumbnailPanel, NewestImageIsFirstAndSelected) {
    ThumbnailPanel panel;
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));

    ASSERT_EQ(panel.count(), 2);
    EXPECT_EQ(idOf(panel.item(0)), 2u);
    EXPECT_EQ(panel.currentRow(), 0);
    ASSERT_FALSE(selected.isEmpty());
    EXPECT_EQ(selected.last().at(0).value<quint64>(), 2u);
}

TEST(ThumbnailPanel, CaptionShowsNameAndSummary) {
    ThumbnailPanel panel;

    panel.addImage(makeEntry(1, "name"));

    EXPECT_EQ(panel.item(0)->text(), QString::fromUtf8("name\n4×3 · 1 ch · 8U"));
}

TEST(ThumbnailPanel, SelectingAnItemReportsItsId) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));
    QSignalSpy selected(&panel, &ThumbnailPanel::imageSelected);

    panel.setCurrentRow(1);

    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(selected.first().at(0).value<quint64>(), 1u);
}

TEST(ThumbnailPanel, RemovesEvictedImage) {
    ThumbnailPanel panel;
    panel.addImage(makeEntry(1, "a"));
    panel.addImage(makeEntry(2, "b"));

    panel.removeImage(1);

    ASSERT_EQ(panel.count(), 1);
    EXPECT_EQ(idOf(panel.item(0)), 2u);
}

} // namespace
