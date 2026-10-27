#pragma once

#include "visual_debug/image_message.h"

#include <opencv2/core/mat.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <variant>

// Wire format version 2: a two-frame ZeroMQ message holding a JSON header and the raw pixels.
// The authoritative description is docs/wire-format.md; change both together.
namespace visual_debug {

inline constexpr int kProtocolVersion = 2;
inline constexpr std::size_t kMaxNameBytes = 1024;
inline constexpr std::size_t kMaxHeaderBytes = 64 * 1024;
inline constexpr std::size_t kMaxPixelBytes = std::size_t{512} * 1024 * 1024;
inline constexpr int kMaxDimension = 32768;
inline constexpr int kMaxChannels = 4;

struct EncodedMessage {
    std::string header;
    cv::Mat pixels; // always continuous; may share data with the image that was encoded

    std::span<const std::byte> pixelBytes() const;
};

// Reasons are fixed ASCII texts, never built from the input, so they are always safe to send back.
struct EncodeError {
    std::string reason;
};
struct DecodeError {
    std::string reason;
};

// True for a non-empty 2-D image of depth 8U to 64F with 1-4 channels, at most kMaxDimension on each
// side and kMaxPixelBytes in total. Reads only the image's metadata.
bool isSupportedImage(const cv::Mat& image);

// Names that are not valid UTF-8 have the bad bytes replaced with U+FFFD, then are cut to kMaxNameBytes
// at a character boundary.
std::variant<EncodedMessage, EncodeError> encode(const cv::Mat& image, std::string_view name);

// Validates every header field and the pixel frame length before allocating. The returned image owns
// its pixels. Never throws.
std::variant<ImageMessage, DecodeError> decode(std::string_view header, std::span<const std::byte> pixels);

} // namespace visual_debug
