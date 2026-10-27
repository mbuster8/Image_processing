#pragma once

#include <opencv2/core/mat.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace viewer_core {

using ImageId = std::uint64_t;

inline constexpr std::size_t kDefaultHistoryCapacity = 100;
// 100 frames of 4K video, original plus display copy, would need about 11 GiB.
inline constexpr std::size_t kDefaultHistoryByteBudget = std::size_t{2} * 1024 * 1024 * 1024;

struct ReceivedImage {
    ImageId id;
    std::string name;
    cv::Mat original; // as received; the pixel readout uses this
    cv::Mat display;  // 8-bit RGBA from toDisplayImage
    std::string summary;

    std::size_t byteSize() const; // original plus display pixels
};

// "640×480 · 3 ch · 8U": columns × rows, channels, depth.
std::string describeImage(const cv::Mat& image);

// The viewer's list of images, newest first, bounded by a count and by the bytes of pixels it holds. Adding
// evicts the oldest entries until both limits hold, but always keeps the image just added.
class ImageHistory {
public:
    struct AddResult {
        ImageId id;
        std::vector<ImageId> evicted; // oldest first, so a UI can drop the matching thumbnails
    };

    // Throws std::invalid_argument if either limit is zero.
    explicit ImageHistory(std::size_t capacity = kDefaultHistoryCapacity,
                          std::size_t byteBudget = kDefaultHistoryByteBudget);

    // Keeps a reference to `original`. Throws std::invalid_argument, leaving the history unchanged, for an
    // image toDisplayImage cannot show.
    AddResult add(std::string name, cv::Mat original);

    const ReceivedImage* find(ImageId id) const; // nullptr once evicted
    const std::deque<ReceivedImage>& entries() const;
    std::size_t size() const;
    std::size_t totalBytes() const;

private:
    std::size_t m_capacity;
    std::size_t m_byteBudget;
    std::deque<ReceivedImage> m_entries;
    std::size_t m_totalBytes = 0;
    ImageId m_nextId = 1;
};

} // namespace viewer_core
