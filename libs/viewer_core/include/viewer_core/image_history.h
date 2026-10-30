#pragma once

#include <opencv2/core/mat.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace viewer_core {

using ImageId = std::uint64_t;

inline constexpr std::size_t kDefaultHistoryCapacity = 100;
// 100 frames of 4K video, original plus display copy, would need about 11 GiB.
inline constexpr std::size_t kDefaultHistoryByteBudget = std::size_t{2} * 1024 * 1024 * 1024;

// A LiveSnapshot is a live frame copied into the history; like live frames, it is never stored on disk.
enum class ImageSource { Received, OpenedFile, LiveSnapshot };

// Where an entry came from, and its times. The times live here so that storage keeps them with the rest.
struct ImageOrigin {
    ImageSource source = ImageSource::Received;
    std::optional<std::uint64_t> fileBytes;             // size on disk, for an opened file
    std::chrono::system_clock::time_point arrivedAt{};  // received or opened
    std::chrono::system_clock::time_point modifiedAt{}; // last rename or duplication; arrivedAt until then
};

struct ReceivedImage {
    ImageId id;
    std::string name;
    cv::Mat original; // as received; the pixel readout uses this
    cv::Mat display;  // 8-bit RGBA from toDisplayImage
    std::string summary;
    ImageOrigin origin{};

    std::size_t byteSize() const;       // original plus display pixels
    std::size_t pixelDataBytes() const; // the original's pixels only, as the sender sent them
};

// "640×480 · 3 ch · 8U": columns × rows, channels, depth.
std::string describeImage(const cv::Mat& image);

// What an entry for this image counts towards the byte budget: its pixels and its display copy's.
std::size_t entryBytes(const cv::Mat& original);

// The viewer's list of images, newest first, bounded by a count and by the bytes of pixels it holds. Every
// operation that adds an entry evicts the oldest entries until both limits hold, but never evicts the entries
// that operation protects: the entry it placed and, for a duplicate, the original too.
class ImageHistory {
public:
    using Clock = std::chrono::system_clock;

    struct AddResult {
        ImageId id;
        std::vector<ImageId> evicted;         // oldest first, so a UI can drop the matching thumbnails
        std::vector<ImageId> droppedFromUndo; // deleted entries that can no longer be restored
    };

    // Throws std::invalid_argument if either limit is zero. maxNameBytes bounds the names duplicate() makes;
    // every other name comes from the caller, who bounds it.
    explicit ImageHistory(std::size_t capacity = kDefaultHistoryCapacity,
                          std::size_t byteBudget = kDefaultHistoryByteBudget,
                          std::size_t maxNameBytes = std::numeric_limits<std::size_t>::max());

    // Keeps a reference to `original`. Throws std::invalid_argument, leaving the history unchanged, for an
    // image toDisplayImage cannot show. A modifiedAt left at the epoch becomes arrivedAt.
    AddResult add(std::string name, cv::Mat original, ImageOrigin origin = {});

    // Sets the name as given (the caller trims and cuts it) and the modified time. False for an unknown id.
    bool rename(ImageId id, std::string name, Clock::time_point now = Clock::now());

    // Inserts "<name> copy" (then "<name> copy 2", …) directly above the original, sharing its pixels. The
    // name is unlike any in the history or the undo buffer, and the original's name is cut at a character
    // boundary when the whole would exceed maxNameBytes. The copy's bytes still count towards the budget,
    // which overstates the memory used: the safe direction. The copy keeps the original's arrival time; its
    // modified time is `now`. Nothing for an unknown id.
    std::optional<AddResult> duplicate(ImageId id, Clock::time_point now = Clock::now());

    // Deletion keeps the last kUndoDepth deleted entries. They count towards the byte budget, and are the
    // first to go when an operation needs room.
    static constexpr std::size_t kUndoDepth = 10;

    // Moves the entry into the undo buffer. False for an unknown id.
    bool remove(ImageId id);

    // Puts the most recently removed entry back, with its old id: directly below its newer neighbour if that
    // is still here, otherwise directly above its older neighbour, otherwise at its old index (clamped). The
    // result's id is the restored id. Nothing when the undo buffer is empty.
    std::optional<AddResult> restoreLastRemoved();
    std::size_t undoDepth() const;

    // Empties the history and the undo buffer. Ids go on counting, so an old id never names a new entry.
    void clear();

    // For images restored from disk: adds below everything, but only if the entry fits as it is, without
    // evicting anything or dropping undo entries; otherwise nothing. The origin's times are kept as given.
    std::optional<AddResult> addOldest(std::string name, cv::Mat original, ImageOrigin origin);

    const ReceivedImage* find(ImageId id) const; // nullptr once evicted or removed
    const std::deque<ReceivedImage>& entries() const;
    std::size_t size() const;
    std::size_t totalBytes() const; // the history plus the undo buffer

private:
    struct RemovedEntry {
        ReceivedImage entry;
        std::optional<ImageId> newerNeighbour; // at the time of removal
        std::optional<ImageId> olderNeighbour;
        std::size_t index;
    };

    ReceivedImage makeEntry(std::string name, cv::Mat original, ImageOrigin origin);
    void evictUnprotected(std::initializer_list<ImageId> protectedIds, AddResult& result);
    void dropOldestUndoEntry(AddResult* result);

    std::size_t m_capacity;
    std::size_t m_byteBudget;
    std::size_t m_maxNameBytes;
    std::deque<ReceivedImage> m_entries;
    std::deque<RemovedEntry> m_undo; // most recently removed first
    std::size_t m_totalBytes = 0;
    ImageId m_nextId = 1;
};

} // namespace viewer_core
