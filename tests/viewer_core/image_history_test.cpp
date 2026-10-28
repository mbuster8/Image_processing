#include "viewer_core/image_history.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <stdexcept>
#include <vector>

namespace {

using Clock = std::chrono::system_clock;
using viewer_core::ImageHistory;
using viewer_core::ImageId;
using viewer_core::ImageOrigin;
using viewer_core::ImageSource;

Clock::time_point at(int seconds) { return Clock::time_point(std::chrono::seconds(seconds)); }

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

// --- Origin and times
// ----------------------------------------------------------------------------------------

TEST(ImageHistory, RecordsOrigin) {
    ImageHistory history;

    const auto id = history.add("f", greyImage(), {ImageSource::OpenedFile, 1234, at(100)}).id;

    const auto& origin = history.find(id)->origin;
    EXPECT_EQ(origin.source, ImageSource::OpenedFile);
    EXPECT_EQ(origin.fileBytes, 1234u);
    EXPECT_EQ(origin.arrivedAt, at(100));
}

TEST(ImageHistory, EntryBytesAreWhatAnEntryCounts) {
    ImageHistory history;
    const cv::Mat image(5, 7, CV_16UC3, cv::Scalar::all(1));

    history.add("a", image);

    EXPECT_EQ(viewer_core::entryBytes(image), history.totalBytes());
}

TEST(ImageHistory, PixelDataBytesCountsOriginalOnly) {
    ImageHistory history;

    const auto id = history.add("s", cv::Mat(480, 640, CV_16SC1, cv::Scalar(0))).id;

    EXPECT_EQ(history.find(id)->pixelDataBytes(), 614'400u);
}

TEST(ImageHistory, ModifiedTimeStartsAtArrival) {
    ImageHistory history;

    const auto id = history.add("a", greyImage(), {.arrivedAt = at(100)}).id;

    EXPECT_EQ(history.find(id)->origin.modifiedAt, at(100));
}

// --- Rename ------------------------------------------------------------------------------------------------

TEST(ImageHistory, RenamesEntry) {
    ImageHistory history;
    const auto id = history.add("old", greyImage()).id;
    const auto summary = history.find(id)->summary;

    EXPECT_TRUE(history.rename(id, "new"));

    EXPECT_EQ(history.find(id)->name, "new");
    EXPECT_EQ(history.find(id)->summary, summary);
}

TEST(ImageHistory, RenameOfUnknownIdReturnsFalse) {
    ImageHistory history;
    history.add("a", greyImage());

    EXPECT_FALSE(history.rename(99, "x"));
    EXPECT_EQ(history.entries().front().name, "a");
}

TEST(ImageHistory, RenameSetsModifiedTime) {
    ImageHistory history;
    const auto id = history.add("a", greyImage(), {.arrivedAt = at(100)}).id;

    history.rename(id, "x", at(200));

    EXPECT_EQ(history.find(id)->origin.modifiedAt, at(200));
    EXPECT_EQ(history.find(id)->origin.arrivedAt, at(100));
}

// --- Duplicate ---------------------------------------------------------------------------------------------

TEST(ImageHistory, DuplicatePlacesCopyDirectlyAboveOriginal) {
    ImageHistory history;
    history.add("a", greyImage());
    history.add("b", greyImage());
    history.add("c", greyImage());

    const auto result = history.duplicate(2);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 4u);
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 4, 2, 1}));
    EXPECT_EQ(history.find(4)->name, "b copy");
    EXPECT_EQ(history.find(4)->original.data, history.find(2)->original.data);
}

TEST(ImageHistory, DuplicateNamesFurtherCopiesWithNumbers) {
    ImageHistory history;
    history.add("a", greyImage());
    history.add("b", greyImage());

    const auto first = history.duplicate(2);
    const auto second = history.duplicate(2);

    ASSERT_TRUE(first && second);
    EXPECT_EQ(history.find(first->id)->name, "b copy");
    EXPECT_EQ(history.find(second->id)->name, "b copy 2");
}

// The copy's name obeys the same limit as any other, cut at a character boundary so that its suffix stays.
TEST(ImageHistory, DuplicateNameStaysWithinTheNameLimit) {
    ImageHistory history(viewer_core::kDefaultHistoryCapacity, viewer_core::kDefaultHistoryByteBudget, 10);
    const auto ascii = history.add("abcdefgh", greyImage()).id;
    const auto accented = history.add("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", greyImage()).id; // ééééé

    const auto asciiCopy = history.duplicate(ascii);
    const auto accentedCopy = history.duplicate(accented);
    const auto accentedCopy2 = history.duplicate(accented);

    ASSERT_TRUE(asciiCopy && accentedCopy && accentedCopy2);
    EXPECT_EQ(history.find(asciiCopy->id)->name, "abcde copy");
    EXPECT_EQ(history.find(accentedCopy->id)->name, "\xC3\xA9\xC3\xA9 copy");
    EXPECT_EQ(history.find(accentedCopy2->id)->name, "\xC3\xA9 copy 2");
}

// A deleted copy can come back with Undo, so a new copy does not take its name.
TEST(ImageHistory, DuplicateAvoidsNamesUndoCanRestore) {
    ImageHistory history;
    const auto id = history.add("a", greyImage()).id;
    const auto first = history.duplicate(id);
    ASSERT_TRUE(first.has_value());
    history.remove(first->id);

    const auto second = history.duplicate(id);
    history.restoreLastRemoved();

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(history.find(first->id)->name, "a copy");
    EXPECT_EQ(history.find(second->id)->name, "a copy 2");
}

TEST(ImageHistory, DuplicateGetsItsOwnModifiedTime) {
    ImageHistory history;
    const auto id = history.add("a", greyImage(), {.arrivedAt = at(100)}).id;

    const auto copy = history.duplicate(id, at(300));

    ASSERT_TRUE(copy.has_value());
    EXPECT_EQ(history.find(copy->id)->origin.modifiedAt, at(300));
    EXPECT_EQ(history.find(copy->id)->origin.arrivedAt, at(100));
    EXPECT_EQ(history.find(id)->origin.modifiedAt, at(100));
    EXPECT_EQ(history.find(id)->origin.arrivedAt, at(100));
}

TEST(ImageHistory, DuplicateOfOldestAtCapacityKeepsOriginalAndCopy) {
    ImageHistory history(3);
    history.add("a", greyImage());
    history.add("b", greyImage());
    history.add("c", greyImage());

    const auto result = history.duplicate(1);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->evicted, (std::vector<ImageId>{2}));
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 4, 1}));
}

// --- Delete and undo ---------------------------------------------------------------------------------------

ImageHistory historyOfThree(std::size_t capacity = 100) {
    ImageHistory history(capacity);
    history.add("a", greyImage());
    history.add("b", greyImage());
    history.add("c", greyImage());
    return history;
}

TEST(ImageHistory, RemoveMovesEntryToUndoBuffer) {
    auto history = historyOfThree();
    const auto bytes = history.totalBytes();

    EXPECT_TRUE(history.remove(2));

    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 1}));
    EXPECT_EQ(history.undoDepth(), 1u);
    EXPECT_EQ(history.find(2), nullptr);
    EXPECT_EQ(history.totalBytes(), bytes);
}

TEST(ImageHistory, RestoreBringsBackLastRemovedAtItsPlace) {
    auto history = historyOfThree();
    history.remove(2);

    const auto restored = history.restoreLastRemoved();

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->id, 2u);
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 2, 1}));
    EXPECT_EQ(history.find(2)->name, "b");
    EXPECT_EQ(history.undoDepth(), 0u);
}

TEST(ImageHistory, RestoresInReverseOrderBelowNewerNeighbours) {
    auto history = historyOfThree();
    history.remove(2);
    history.remove(1);

    history.restoreLastRemoved();
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 1}));

    history.restoreLastRemoved();
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 2, 1}));
}

TEST(ImageHistory, RestoreGoesAboveOlderNeighbourWhenNewerIsGone) {
    auto history = historyOfThree();
    history.remove(3);
    history.remove(2);

    history.restoreLastRemoved();

    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{2, 1}));
}

TEST(ImageHistory, RestoreFallsBackToClampedIndex) {
    ImageHistory history;
    history.add("a", greyImage());
    history.add("b", greyImage());
    history.remove(1);
    history.remove(2);
    EXPECT_EQ(history.add("c", greyImage()).id, 3u);

    history.restoreLastRemoved();

    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{2, 3}));
}

TEST(ImageHistory, UndoBufferHoldsTenEntries) {
    ImageHistory history;
    for (int i = 0; i < 12; ++i) {
        history.add("image", greyImage());
    }
    const auto bytesOfOne = history.entries().front().byteSize();

    for (ImageId id = 1; id <= 11; ++id) {
        history.remove(id);
    }

    EXPECT_EQ(history.undoDepth(), 10u);
    EXPECT_EQ(history.totalBytes(), 11 * bytesOfOne); // one left in the history, ten in the undo buffer
}

TEST(ImageHistory, UndoEntriesAreDroppedFirstWhenBudgetNeedsRoom) {
    // A 100×100 8UC1 entry costs 50 000 bytes: 10 000 of original plus 40 000 of RGBA display.
    ImageHistory history(100, 3 * 50'000);
    history.add("a", greyImage(100, 100));
    history.add("b", greyImage(100, 100));
    history.add("c", greyImage(100, 100));
    history.remove(2);

    const auto result = history.add("d", greyImage(100, 100));

    EXPECT_EQ(result.droppedFromUndo, (std::vector<ImageId>{2}));
    EXPECT_TRUE(result.evicted.empty());
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{4, 3, 1}));
    EXPECT_EQ(history.undoDepth(), 0u);
}

TEST(ImageHistory, RestoreAtCapacityEvictsNextOldestNotTheRestored) {
    auto history = historyOfThree(3);
    history.remove(1);
    history.add("d", greyImage());
    ASSERT_EQ(idsOf(history), (std::vector<ImageId>{4, 3, 2}));

    const auto restored = history.restoreLastRemoved();

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->evicted, (std::vector<ImageId>{2}));
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{4, 3, 1}));
}

TEST(ImageHistory, RemoveOfUnknownIdReturnsFalse) {
    auto history = historyOfThree();

    EXPECT_FALSE(history.remove(99));
    EXPECT_EQ(history.undoDepth(), 0u);
}

TEST(ImageHistory, RestoreWithEmptyBufferReturnsNothing) {
    auto history = historyOfThree();

    EXPECT_FALSE(history.restoreLastRemoved().has_value());
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{3, 2, 1}));
}

// --- Adding at the oldest end (images restored from disk) -------------------------------------------------

TEST(ImageHistory, AddOldestAppendsBelowEverything) {
    ImageHistory history;
    history.add("a", greyImage());
    history.add("b", greyImage());

    const auto added = history.addOldest("stored", greyImage(), {.arrivedAt = at(50), .modifiedAt = at(60)});

    ASSERT_TRUE(added.has_value());
    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{2, 1, 3}));
    EXPECT_EQ(history.find(3)->name, "stored");
    EXPECT_EQ(history.find(3)->origin.modifiedAt, at(60));
}

TEST(ImageHistory, AddOldestRefusesWhenItDoesNotFit) {
    ImageHistory history(2);
    history.add("a", greyImage());
    history.add("b", greyImage());
    const auto bytes = history.totalBytes();

    EXPECT_FALSE(history.addOldest("stored", greyImage(), {}).has_value());

    EXPECT_EQ(idsOf(history), (std::vector<ImageId>{2, 1}));
    EXPECT_EQ(history.totalBytes(), bytes);
}

// Clear History: nothing stays, not even in the undo buffer, and ids are never reused, so an id the UI still
// holds can never name a new image.
TEST(ImageHistory, ClearEmptiesEntriesAndUndoButKeepsCountingIds) {
    ImageHistory history;
    history.add("a", greyImage());
    const ImageId removed = history.add("b", greyImage()).id;
    history.remove(removed);

    history.clear();

    EXPECT_EQ(history.size(), 0U);
    EXPECT_EQ(history.undoDepth(), 0U);
    EXPECT_EQ(history.totalBytes(), 0U);
    EXPECT_FALSE(history.restoreLastRemoved().has_value());
    EXPECT_EQ(history.add("c", greyImage()).id, 3U);
}

TEST(ImageHistory, DuplicateOfUnknownIdReturnsNothing) {
    ImageHistory history;
    history.add("a", greyImage());

    EXPECT_FALSE(history.duplicate(99).has_value());
    EXPECT_EQ(history.size(), 1u);
}

} // namespace
