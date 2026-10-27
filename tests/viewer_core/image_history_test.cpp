#include "viewer_core/image_history.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <stdexcept>
#include <vector>

namespace {

using viewer_core::ImageHistory;
using viewer_core::ImageId;

cv::Mat greyImage(int rows = 2, int cols = 2) { return cv::Mat(rows, cols, CV_8UC1, cv::Scalar(1)); }

std::vector<ImageId> idsOf(const ImageHistory& history) {
    std::vector<ImageId> ids;
    for (const auto& entry : history.entries()) {
        ids.push_back(entry.id);
    }
    return ids;
}

TEST(ImageHistory, AssignsIncreasingIdsNewestFirst) {
    ImageHistory history;

    EXPECT_EQ(history.add("a", greyImage()).id, 1u);
    EXPECT_EQ(history.add("b", greyImage()).id, 2u);

    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{2, 1}));
    EXPECT_EQ(history.entries().front().name, "b");
}

TEST(ImageHistory, EvictsOldestAtCapacity) {
    ImageHistory history(2);
    history.add("a", greyImage());
    history.add("b", greyImage());

    const auto result = history.add("c", greyImage());

    EXPECT_EQ(result.evicted, (std::vector<ImageId>{1}));
    EXPECT_EQ(history.find(1), nullptr);
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 2}));
}

TEST(ImageHistory, EvictsSeveralToStayWithinBudget) {
    // A 100×100 8UC1 entry costs 10 000 bytes of original plus 40 000 of RGBA display.
    ImageHistory history(100, 100'000);
    history.add("a", greyImage(100, 100));
    history.add("b", greyImage(100, 100));

    const auto result = history.add("c", greyImage(200, 100)); // 100 000 bytes on its own

    EXPECT_EQ(result.evicted, (std::vector<ImageId>{1, 2}));
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3}));
    EXPECT_EQ(history.totalBytes(), 100'000u);
}

TEST(ImageHistory, KeepsNewestEvenOverBudget) {
    ImageHistory history(100, 1);

    const auto first = history.add("a", greyImage());
    EXPECT_TRUE(first.evicted.empty());
    EXPECT_EQ(history.size(), 1u);

    const auto second = history.add("b", greyImage());
    EXPECT_EQ(second.evicted, (std::vector<ImageId>{1}));
    EXPECT_EQ(history.size(), 1u);
}

TEST(ImageHistory, StoresOriginalDisplayAndSummary) {
    ImageHistory history;

    const auto id = history.add("depth", cv::Mat(3, 4, CV_16UC1, cv::Scalar(500))).id;

    const auto* entry = history.find(id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->original.type(), CV_16UC1);
    EXPECT_EQ(entry->display.type(), CV_8UC4);
    EXPECT_EQ(entry->summary, "4×3 · 1 ch · 16U");
    EXPECT_EQ(entry->name, "depth");
}

TEST(ImageHistory, RejectsZeroLimits) {
    EXPECT_THROW(ImageHistory(0, 1), std::invalid_argument);
    EXPECT_THROW(ImageHistory(1, 0), std::invalid_argument);
}

TEST(ImageHistory, LeavesHistoryUnchangedWhenImageIsUnsupported) {
    ImageHistory history;
    history.add("a", greyImage());

    EXPECT_THROW(history.add("bad", cv::Mat()), std::invalid_argument);

    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{1}));
}

} // namespace
