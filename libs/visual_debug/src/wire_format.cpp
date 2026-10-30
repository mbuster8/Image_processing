#include "visual_debug/wire_format.h"

#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>

namespace visual_debug {
namespace {

using Json = nlohmann::json;

constexpr std::string_view kProtocolName = "visual_debug";
constexpr std::string_view kImageType = "image";

struct DepthInfo {
    int depth;
    std::string_view name;
    std::size_t elementSize;
};

// The protocol's own table, deliberately not derived from OpenCV: OpenCV 5 adds depths (16F, 16BF, Bool,
// 64U, 64S, 32U) that version 2 of the format does not carry, and the names are part of the wire contract.
constexpr std::array<DepthInfo, 7> kDepths{{
    {CV_8U, "8U", 1},
    {CV_8S, "8S", 1},
    {CV_16U, "16U", 2},
    {CV_16S, "16S", 2},
    {CV_32S, "32S", 4},
    {CV_32F, "32F", 4},
    {CV_64F, "64F", 8},
}};

const DepthInfo* findDepth(int depth) {
    const auto it =
        std::find_if(kDepths.begin(), kDepths.end(), [&](const auto& d) { return d.depth == depth; });
    return it == kDepths.end() ? nullptr : &*it;
}

const DepthInfo* findDepth(std::string_view name) {
    const auto it =
        std::find_if(kDepths.begin(), kDepths.end(), [&](const auto& d) { return d.name == name; });
    return it == kDepths.end() ? nullptr : &*it;
}

// At most 32768 × 32768 × 4 channels × 8 bytes = 2^35, so the product cannot overflow 64 bits.
std::uint64_t pixelByteCount(int rows, int cols, int channels, const DepthInfo& depth) {
    return std::uint64_t(rows) * std::uint64_t(cols) * std::uint64_t(channels) * depth.elementSize;
}

// Length of the well-formed UTF-8 sequence starting at text[i], or 0 if there is none (RFC 3629: no
// overlong forms, no surrogates, nothing above U+10FFFF).
std::size_t utf8SequenceLength(std::string_view text, std::size_t i) {
    const auto byteAt = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
    const unsigned char lead = byteAt(i);
    if (lead < 0x80) {
        return 1;
    }
    std::size_t length = 0;
    unsigned char secondMin = 0x80;
    unsigned char secondMax = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
    } else if (lead == 0xE0) {
        length = 3;
        secondMin = 0xA0;
    } else if ((lead >= 0xE1 && lead <= 0xEC) || lead == 0xEE || lead == 0xEF) {
        length = 3;
    } else if (lead == 0xED) {
        length = 3;
        secondMax = 0x9F;
    } else if (lead == 0xF0) {
        length = 4;
        secondMin = 0x90;
    } else if (lead >= 0xF1 && lead <= 0xF3) {
        length = 4;
    } else if (lead == 0xF4) {
        length = 4;
        secondMax = 0x8F;
    } else {
        return 0;
    }
    if (i + length > text.size() || byteAt(i + 1) < secondMin || byteAt(i + 1) > secondMax) {
        return 0;
    }
    for (std::size_t k = 2; k < length; ++k) {
        if ((byteAt(i + k) & 0xC0) != 0x80) {
            return 0;
        }
    }
    return length;
}

// Windows file names in an ANSI code page arrive here as invalid UTF-8, which JSON cannot carry.
std::string toValidUtf8(std::string_view text) {
    constexpr std::string_view kReplacementCharacter = "\xEF\xBF\xBD"; // U+FFFD
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        if (const auto length = utf8SequenceLength(text, i); length > 0) {
            result.append(text.substr(i, length));
            i += length;
        } else {
            result.append(kReplacementCharacter);
            ++i;
        }
    }
    return result;
}

// Expects valid UTF-8; steps back over continuation bytes so no character is cut in half.
std::string truncateUtf8(std::string text, std::size_t maxBytes) {
    if (text.size() <= maxBytes) {
        return text;
    }
    std::size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    text.resize(cut);
    return text;
}

// The parsed header is only ever inspected in place: copying or dumping a deeply nested document recurses
// once per level and can overflow a secondary thread's stack.

bool hasString(const Json& header, const char* key, std::string_view expected) {
    const auto it = header.find(key);
    return it != header.end() && it->is_string() && it->get_ref<const std::string&>() == expected;
}

// JSON integers only: booleans and floating-point numbers are rejected. The value is range-checked as a
// 64-bit integer, before any narrowing.
std::optional<std::int64_t> readInteger64(const Json& header, const char* key, std::int64_t min,
                                          std::int64_t max) {
    const auto it = header.find(key);
    if (it == header.end() || !it->is_number_integer()) {
        return std::nullopt;
    }
    if (it->is_number_unsigned()) {
        const auto value = it->get<std::uint64_t>();
        if (value > static_cast<std::uint64_t>(max) || static_cast<std::int64_t>(value) < min) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(value);
    }
    const auto value = it->get<std::int64_t>();
    if (value < min || value > max) {
        return std::nullopt;
    }
    return value;
}

// Checked before narrowing, so 4294967297 cannot wrap to 1.
std::optional<int> readInteger(const Json& header, const char* key, int min, int max) {
    const auto value = readInteger64(header, key, min, max);
    return value ? std::optional<int>(static_cast<int>(*value)) : std::nullopt;
}

std::variant<ImageMessage, DecodeError> decodeChecked(std::string_view headerText,
                                                      std::span<const std::byte> pixels) {
    if (headerText.size() > kMaxHeaderBytes) {
        return DecodeError{"header too large"};
    }
    // A malformed header is expected input from the network, not a programming error, so no exceptions.
    const Json header = Json::parse(headerText, nullptr, /*allow_exceptions=*/false);
    if (header.is_discarded()) {
        return DecodeError{"header is not valid JSON"};
    }
    if (!header.is_object()) {
        return DecodeError{"header is not a JSON object"};
    }
    if (!hasString(header, "protocol", kProtocolName)) {
        return DecodeError{"unknown protocol"};
    }
    if (readInteger(header, "version", 0, std::numeric_limits<int>::max()) != kProtocolVersion) {
        return DecodeError{"unsupported version"};
    }
    if (!hasString(header, "type", kImageType)) {
        return DecodeError{"unsupported message type"};
    }

    const auto name = header.find("name");
    if (name == header.end() || !name->is_string()) {
        return DecodeError{"name is missing or not a string"};
    }
    const auto& nameText = name->get_ref<const std::string&>();
    if (nameText.size() > kMaxNameBytes) {
        return DecodeError{"name too long"};
    }

    const auto rows = readInteger(header, "rows", 1, kMaxDimension);
    if (!rows) {
        return DecodeError{"rows out of range"};
    }
    const auto cols = readInteger(header, "cols", 1, kMaxDimension);
    if (!cols) {
        return DecodeError{"cols out of range"};
    }
    const auto depthField = header.find("depth");
    const DepthInfo* depth = (depthField != header.end() && depthField->is_string())
                                 ? findDepth(depthField->get_ref<const std::string&>())
                                 : nullptr;
    if (depth == nullptr) {
        return DecodeError{"unsupported depth"};
    }
    const auto channels = readInteger(header, "channels", 1, kMaxChannels);
    if (!channels) {
        return DecodeError{"channels out of range"};
    }

    // Both optional, on any image; an older sender sends neither.
    bool live = false;
    if (const auto field = header.find("live"); field != header.end()) {
        if (!field->is_boolean()) {
            return DecodeError{"live is not a boolean"};
        }
        live = field->get<bool>();
    }
    std::optional<SentTime> sentAt;
    if (header.contains("sentAt")) {
        const auto milliseconds = readInteger64(header, "sentAt", 0, kMaxSentAtMilliseconds);
        if (!milliseconds) {
            return DecodeError{"sentAt out of range"};
        }
        sentAt = SentTime{std::chrono::milliseconds(*milliseconds)};
    }

    const auto expectedBytes = pixelByteCount(*rows, *cols, *channels, *depth);
    if (expectedBytes > kMaxPixelBytes) {
        return DecodeError{"image too large"};
    }
    if (pixels.size() != expectedBytes) {
        return DecodeError{"pixel frame has the wrong size"};
    }

    // cv::Mat has no read-only view type; the clone is the only access, and gives the image its own pixels.
    const cv::Mat view(*rows, *cols, CV_MAKETYPE(depth->depth, *channels),
                       const_cast<std::byte*>(pixels.data()));
    return ImageMessage{.name = nameText, .image = view.clone(), .live = live, .sentAt = sentAt};
}

} // namespace

std::string sanitiseName(std::string_view name) { return truncateUtf8(toValidUtf8(name), kMaxNameBytes); }

std::span<const std::byte> EncodedMessage::pixelBytes() const {
    return {reinterpret_cast<const std::byte*>(pixels.data), pixels.total() * pixels.elemSize()};
}

bool isSupportedImage(const cv::Mat& image) {
    if (image.empty() || image.dims != 2 || image.rows > kMaxDimension || image.cols > kMaxDimension ||
        image.channels() > kMaxChannels) {
        return false;
    }
    const DepthInfo* depth = findDepth(image.depth());
    return depth != nullptr &&
           pixelByteCount(image.rows, image.cols, image.channels(), *depth) <= kMaxPixelBytes;
}

std::variant<EncodedMessage, EncodeError> encode(const cv::Mat& image, std::string_view name,
                                                 const EncodeOptions& options) {
    if (!isSupportedImage(image)) {
        return EncodeError{"image is empty, not 2-D, too large, or of an unsupported depth or channel count"};
    }
    // ordered_json keeps the documented field order, which makes captured traffic easier to read.
    nlohmann::ordered_json header = {
        {"protocol", kProtocolName},
        {"version", kProtocolVersion},
        {"type", kImageType},
        {"name", sanitiseName(name)},
        {"rows", image.rows},
        {"cols", image.cols},
        {"depth", findDepth(image.depth())->name},
        {"channels", image.channels()},
    };
    if (options.live) {
        header["live"] = true;
    }
    // A clock set before 1970 or past 9999 sends no sentAt rather than one the viewer refuses.
    if (options.sentAt) {
        const auto milliseconds = options.sentAt->time_since_epoch().count();
        if (milliseconds >= 0 && milliseconds <= kMaxSentAtMilliseconds) {
            header["sentAt"] = milliseconds;
        }
    }

    EncodedMessage message;
    message.header = header.dump();
    // A region of interest keeps its parent's row stride; the wire carries tightly packed rows.
    message.pixels = image.isContinuous() ? image : image.clone();
    return message;
}

std::variant<ImageMessage, DecodeError> decode(std::string_view header, std::span<const std::byte> pixels) {
    try {
        return decodeChecked(header, pixels);
    } catch (const std::exception&) {
        // Only allocation can fail past validation; report it rather than let it escape a receive loop.
        return DecodeError{"could not decode message"};
    }
}

} // namespace visual_debug
