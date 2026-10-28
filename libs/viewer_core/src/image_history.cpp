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
    return depth >= 0 && depth < static_cast<int>(kNames.size()) ? kNames[static_cast<std::size_t>(depth)]
                                                                 : "?";
}

std::size_t pixelBytes(const cv::Mat& image) { return image.total() * image.elemSize(); }

// Names are valid UTF-8 (every caller sanitises them), so stepping back over continuation bytes finds the
// start of the character the cut would split.
std::string cutAtCharacter(const std::string& text, std::size_t maxBytes) {
    if (text.size() <= maxBytes) {
        return text;
    }
    std::size_t end = maxBytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
        --end;
    }
    return text.substr(0, end);
}

} // namespace

std::size_t entryBytes(const cv::Mat& original) { return pixelBytes(original) + displayBytes(original); }

std::size_t ReceivedImage::byteSize() const { return pixelBytes(original) + pixelBytes(display); }

std::size_t ReceivedImage::pixelDataBytes() const { return pixelBytes(original); }

std::string describeImage(const cv::Mat& image) {
    return std::to_string(image.cols) + "×" + std::to_string(image.rows) + " · " +
           std::to_string(image.channels()) + " ch · " + std::string(depthName(image.depth()));
}

ImageHistory::ImageHistory(std::size_t capacity, std::size_t byteBudget, std::size_t maxNameBytes)
    : m_capacity(capacity), m_byteBudget(byteBudget), m_maxNameBytes(maxNameBytes) {
    if (capacity == 0 || byteBudget == 0) {
        throw std::invalid_argument("ImageHistory: capacity and byte budget must be positive");
    }
}

// Converts before anything changes, so an unsupported image leaves the history as it was. Takes an id only
// once the conversion has succeeded.
ReceivedImage ImageHistory::makeEntry(std::string name, cv::Mat original, ImageOrigin origin) {
    cv::Mat display = toDisplayImage(original);
    std::string summary = describeImage(original);
    if (origin.modifiedAt == Clock::time_point{}) {
        origin.modifiedAt = origin.arrivedAt;
    }
    return ReceivedImage{m_nextId++,         std::move(name),    std::move(original),
                         std::move(display), std::move(summary), origin};
}

ImageHistory::AddResult ImageHistory::add(std::string name, cv::Mat original, ImageOrigin origin) {
    m_entries.push_front(makeEntry(std::move(name), std::move(original), origin));
    m_totalBytes += m_entries.front().byteSize();

    AddResult result{m_entries.front().id, {}, {}};
    evictUnprotected({result.id}, result);
    return result;
}

std::optional<ImageHistory::AddResult> ImageHistory::addOldest(std::string name, cv::Mat original,
                                                               ImageOrigin origin) {
    // Checked on the original's and display's sizes before converting, so a refused image costs nothing.
    if (m_entries.size() >= m_capacity) {
        return std::nullopt;
    }
    if (m_totalBytes + entryBytes(original) > m_byteBudget) {
        return std::nullopt;
    }
    m_entries.push_back(makeEntry(std::move(name), std::move(original), origin));
    m_totalBytes += m_entries.back().byteSize();
    return AddResult{m_entries.back().id, {}, {}};
}

bool ImageHistory::remove(ImageId id) {
    const auto it =
        std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) { return entry.id == id; });
    if (it == m_entries.end()) {
        return false;
    }
    const auto index = static_cast<std::size_t>(it - m_entries.begin());
    RemovedEntry removed{std::move(*it), std::nullopt, std::nullopt, index};
    if (index > 0) {
        removed.newerNeighbour = m_entries[index - 1].id;
    }
    if (index + 1 < m_entries.size()) {
        removed.olderNeighbour = m_entries[index + 1].id;
    }
    m_entries.erase(it);
    m_undo.push_front(std::move(removed)); // its bytes still count, so totalBytes is unchanged
    if (m_undo.size() > kUndoDepth) {
        dropOldestUndoEntry(nullptr);
    }
    return true;
}

std::optional<ImageHistory::AddResult> ImageHistory::restoreLastRemoved() {
    if (m_undo.empty()) {
        return std::nullopt;
    }
    RemovedEntry removed = std::move(m_undo.front());
    m_undo.pop_front();

    const auto indexOf = [&](std::optional<ImageId> id) -> std::optional<std::size_t> {
        if (!id) {
            return std::nullopt;
        }
        const auto it =
            std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& e) { return e.id == *id; });
        if (it == m_entries.end()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(it - m_entries.begin());
    };
    std::size_t position = std::min(removed.index, m_entries.size());
    if (const auto newer = indexOf(removed.newerNeighbour)) {
        position = *newer + 1;
    } else if (const auto older = indexOf(removed.olderNeighbour)) {
        position = *older;
    }

    const ImageId id = removed.entry.id;
    m_entries.insert(m_entries.begin() + static_cast<std::ptrdiff_t>(position), std::move(removed.entry));
    AddResult result{id, {}, {}};
    evictUnprotected({id}, result);
    return result;
}

std::size_t ImageHistory::undoDepth() const { return m_undo.size(); }

void ImageHistory::clear() {
    m_entries.clear();
    m_undo.clear();
    m_totalBytes = 0;
}

void ImageHistory::dropOldestUndoEntry(AddResult* result) {
    m_totalBytes -= m_undo.back().entry.byteSize();
    if (result != nullptr) {
        result->droppedFromUndo.push_back(m_undo.back().entry.id);
    }
    m_undo.pop_back();
}

bool ImageHistory::rename(ImageId id, std::string name, Clock::time_point now) {
    const auto it =
        std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) { return entry.id == id; });
    if (it == m_entries.end()) {
        return false;
    }
    it->name = std::move(name);
    it->origin.modifiedAt = now;
    return true;
}

std::optional<ImageHistory::AddResult> ImageHistory::duplicate(ImageId id, Clock::time_point now) {
    const auto it =
        std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) { return entry.id == id; });
    if (it == m_entries.end()) {
        return std::nullopt;
    }
    const auto nameTaken = [&](const std::string& candidate) {
        const auto named = [&](const ReceivedImage& entry) { return entry.name == candidate; };
        return std::any_of(m_entries.begin(), m_entries.end(), named) ||
               std::any_of(m_undo.begin(), m_undo.end(),
                           [&](const RemovedEntry& removed) { return named(removed.entry); });
    };
    const auto copyName = [&](const std::string& suffix) {
        const std::size_t room = m_maxNameBytes > suffix.size() ? m_maxNameBytes - suffix.size() : 0;
        return cutAtCharacter(it->name, room) + suffix;
    };
    std::string name = copyName(" copy");
    for (int number = 2; nameTaken(name); ++number) {
        name = copyName(" copy " + std::to_string(number));
    }

    ReceivedImage copy = *it; // cv::Mat copies share the pixel buffers
    copy.id = m_nextId++;
    copy.name = std::move(name);
    copy.origin.modifiedAt = now;
    m_totalBytes += copy.byteSize();
    const ImageId copyId = copy.id;
    m_entries.insert(it, std::move(copy)); // before the original: one place newer

    AddResult result{copyId, {}, {}};
    evictUnprotected({copyId, id}, result);
    return result;
}

void ImageHistory::evictUnprotected(std::initializer_list<ImageId> protectedIds, AddResult& result) {
    const auto overLimit = [&] { return m_entries.size() > m_capacity || m_totalBytes > m_byteBudget; };
    const auto isProtected = [&](ImageId id) {
        return std::find(protectedIds.begin(), protectedIds.end(), id) != protectedIds.end();
    };
    // Deleted images give up their room before any image still in the list.
    while (m_totalBytes > m_byteBudget && !m_undo.empty()) {
        dropOldestUndoEntry(&result);
    }
    // From the oldest end towards the newest.
    for (auto it = m_entries.end(); overLimit() && it != m_entries.begin();) {
        --it;
        if (isProtected(it->id)) {
            continue;
        }
        m_totalBytes -= it->byteSize();
        result.evicted.push_back(it->id);
        it = m_entries.erase(it);
    }
}

const ReceivedImage* ImageHistory::find(ImageId id) const {
    const auto it =
        std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) { return entry.id == id; });
    return it == m_entries.end() ? nullptr : &*it;
}

const std::deque<ReceivedImage>& ImageHistory::entries() const { return m_entries; }

std::size_t ImageHistory::size() const { return m_entries.size(); }

std::size_t ImageHistory::totalBytes() const { return m_totalBytes; }

} // namespace viewer_core
