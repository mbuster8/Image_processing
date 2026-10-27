#include "viewer_core/image_history.h"

#include "viewer_core/display_converter.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace viewer_core {
namespace {

std::string_view depthName(int depth) {
    constexpr std::array<std::string_view, 7> kNames{"8U", "8S", "16U", "16S", "32S", "32F", "64F"};
    return depth >= 0 && depth < static_cast<int>(kNames.size()) ? kNames[static_cast<std::size_t>(depth)] : "?";
}

std::size_t pixelBytes(const cv::Mat& image) { return image.total() * image.elemSize(); }

} // namespace

std::size_t ReceivedImage::byteSize() const { return pixelBytes(original) + pixelBytes(display); }

std::string describeImage(const cv::Mat& image) {
    return std::to_string(image.cols) + "×" + std::to_string(image.rows) + " · " +
           std::to_string(image.channels()) + " ch · " + std::string(depthName(image.depth()));
}

ImageHistory::ImageHistory(std::size_t capacity, std::size_t byteBudget)
    : m_capacity(capacity), m_byteBudget(byteBudget) {
    if (capacity == 0 || byteBudget == 0) {
        throw std::invalid_argument("ImageHistory: capacity and byte budget must be positive");
    }
}

ImageHistory::AddResult ImageHistory::add(std::string name, cv::Mat original) {
    // Converted before anything changes, so an unsupported image leaves the history as it was.
    cv::Mat display = toDisplayImage(original);
    std::string summary = describeImage(original);

    const ImageId id = m_nextId++;
    m_entries.push_front(ReceivedImage{id, std::move(name), std::move(original), std::move(display), std::move(summary)});
    m_totalBytes += m_entries.front().byteSize();

    AddResult result{id, {}};
    while (m_entries.size() > 1 && (m_entries.size() > m_capacity || m_totalBytes > m_byteBudget)) {
        m_totalBytes -= m_entries.back().byteSize();
        result.evicted.push_back(m_entries.back().id);
        m_entries.pop_back();
    }
    return result;
}

const ReceivedImage* ImageHistory::find(ImageId id) const {
    const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) { return entry.id == id; });
    return it == m_entries.end() ? nullptr : &*it;
}

const std::deque<ReceivedImage>& ImageHistory::entries() const { return m_entries; }

std::size_t ImageHistory::size() const { return m_entries.size(); }

std::size_t ImageHistory::totalBytes() const { return m_totalBytes; }

} // namespace viewer_core
