#include "visual_debug/wire_format.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace std::chrono_literals;
using visual_debug::DecodeError;
using visual_debug::EncodedMessage;
using visual_debug::EncodeError;
using visual_debug::ImageMessage;
using visual_debug::SentTime;

constexpr SentTime kSentAt{1'700'000'000'123ms};

EncodedMessage encodeOrFail(const cv::Mat& image, std::string_view name,
                            const visual_debug::EncodeOptions& options = {}) {
    auto result = visual_debug::encode(image, name, options);
    if (const auto* error = std::get_if<EncodeError>(&result)) {
        ADD_FAILURE() << "encode failed: " << error->reason;
        return {};
    }
    return std::get<EncodedMessage>(std::move(result));
}

ImageMessage roundTrip(const cv::Mat& image, std::string_view name,
                       const visual_debug::EncodeOptions& options = {}) {
    const auto encoded = encodeOrFail(image, name, options);
    auto decoded = visual_debug::decode(encoded.header, encoded.pixelBytes());
    if (const auto* error = std::get_if<DecodeError>(&decoded)) {
        ADD_FAILURE() << "decode failed: " << error->reason;
        return {};
    }
    return std::get<ImageMessage>(std::move(decoded));
}

bool isAscii(std::string_view text) {
    return std::all_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
}

std::string depthLabel(int depth) {
    constexpr std::array<const char*, 7> kLabels{"8U", "8S", "16U", "16S", "32S", "32F", "64F"};
    return kLabels.at(static_cast<std::size_t>(depth));
}

// --- Round trips -------------------------------------------------------------------------------------------

class WireFormatRoundTrip : public testing::TestWithParam<std::tuple<int, int>> {};

TEST_P(WireFormatRoundTrip, RoundTripsEveryDepthAndChannelCount) {
    const auto [depth, channels] = GetParam();
    cv::Mat image(5, 7, CV_MAKETYPE(depth, channels));
    cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(100));

    const auto decoded = roundTrip(image, "random");

    ASSERT_EQ(decoded.image.type(), image.type());
    ASSERT_EQ(decoded.image.size(), image.size());
    EXPECT_EQ(cv::norm(decoded.image, image, cv::NORM_INF), 0.0);
    EXPECT_EQ(decoded.name, "random");
}

INSTANTIATE_TEST_SUITE_P(AllDepthsAndChannels, WireFormatRoundTrip,
                         testing::Combine(testing::Values(CV_8U, CV_8S, CV_16U, CV_16S, CV_32S, CV_32F,
                                                          CV_64F),
                                          testing::Range(1, 5)),
                         [](const testing::TestParamInfo<WireFormatRoundTrip::ParamType>& info) {
                             return depthLabel(std::get<0>(info.param)) + "C" +
                                    std::to_string(std::get<1>(info.param));
                         });

TEST(WireFormat, HeaderIsJsonWithSpecFields) {
    const cv::Mat image(5, 7, CV_16SC3, cv::Scalar::all(1));

    const auto encoded = encodeOrFail(image, "Sobel X");
    const auto header = nlohmann::json::parse(encoded.header);

    EXPECT_EQ(header.at("protocol"), "visual_debug");
    EXPECT_EQ(header.at("version"), 2);
    EXPECT_EQ(header.at("type"), "image");
    EXPECT_EQ(header.at("name"), "Sobel X");
    EXPECT_EQ(header.at("rows"), 5);
    EXPECT_EQ(header.at("cols"), 7);
    EXPECT_EQ(header.at("depth"), "16S");
    EXPECT_EQ(header.at("channels"), 3);
    EXPECT_EQ(encoded.pixelBytes().size(), 5u * 7u * 3u * 2u);
}

TEST(WireFormat, EncodesRegionOfInterestContiguously) {
    cv::Mat big(10, 10, CV_8UC3);
    cv::randu(big, cv::Scalar::all(0), cv::Scalar::all(255));
    const cv::Mat roi = big(cv::Rect(2, 3, 4, 5));
    ASSERT_FALSE(roi.isContinuous());

    EXPECT_EQ(encodeOrFail(roi, "roi").pixelBytes().size(), 4u * 5u * 3u);

    const auto decoded = roundTrip(roi, "roi");
    ASSERT_EQ(decoded.image.size(), roi.size());
    EXPECT_EQ(cv::norm(decoded.image, roi, cv::NORM_INF), 0.0);
}

// --- Live frames -------------------------------------------------------------------------------------------

TEST(WireFormat, EncodesLiveAndSentAtForLiveFrames) {
    const cv::Mat image(2, 3, CV_8UC1, cv::Scalar(9));

    const auto encoded = encodeOrFail(image, "camera", {.live = true, .sentAt = kSentAt});
    const auto header = nlohmann::ordered_json::parse(encoded.header);

    EXPECT_EQ(header.at("live"), true);
    EXPECT_EQ(header.at("sentAt"), 1700000000123);
    std::vector<std::string> keys;
    for (const auto& item : header.items()) {
        keys.push_back(item.key());
    }
    ASSERT_GE(keys.size(), 2u);
    EXPECT_EQ(keys[keys.size() - 2], "live");
    EXPECT_EQ(keys.back(), "sentAt");
}

TEST(WireFormat, OrdinaryImagesCarryNeither) {
    const cv::Mat image(2, 3, CV_8UC1, cv::Scalar(9));

    const auto header = nlohmann::json::parse(encodeOrFail(image, "x").header);

    EXPECT_FALSE(header.contains("live"));
    EXPECT_FALSE(header.contains("sentAt"));
}

TEST(WireFormat, DecodesLiveAndSentAt) {
    const cv::Mat image(2, 3, CV_8UC1, cv::Scalar(9));

    const auto decoded = roundTrip(image, "camera", {.live = true, .sentAt = kSentAt});

    EXPECT_TRUE(decoded.live);
    EXPECT_EQ(decoded.sentAt, kSentAt);
}

// An older sender's header, and a new sender's still.
TEST(WireFormat, HeaderWithoutLiveDecodesAsOrdinaryImage) {
    const std::vector<std::byte> pixels(2 * 3);
    const nlohmann::ordered_json header = {{"protocol", "visual_debug"},
                                           {"version", 2},
                                           {"type", "image"},
                                           {"name", "test"},
                                           {"rows", 2},
                                           {"cols", 3},
                                           {"depth", "8U"},
                                           {"channels", 1}};

    const auto decoded = visual_debug::decode(header.dump(), pixels);

    ASSERT_TRUE(std::holds_alternative<ImageMessage>(decoded));
    EXPECT_FALSE(std::get<ImageMessage>(decoded).live);
    EXPECT_FALSE(std::get<ImageMessage>(decoded).sentAt.has_value());
}

// A sentAt that cannot be carried is left out rather than refused: the frame is still worth showing.
TEST(WireFormat, OmitsAnOutOfRangeSentAtOnEncode) {
    const cv::Mat image(2, 3, CV_8UC1, cv::Scalar(9));

    for (const SentTime sentAt :
         {SentTime{-1ms}, SentTime{std::chrono::milliseconds(visual_debug::kMaxSentAtMilliseconds + 1)}}) {
        const auto header =
            nlohmann::json::parse(encodeOrFail(image, "camera", {.live = true, .sentAt = sentAt}).header);
        EXPECT_EQ(header.at("live"), true);
        EXPECT_FALSE(header.contains("sentAt")) << sentAt.time_since_epoch().count();
    }
}

// --- Names -------------------------------------------------------------------------------------------------

TEST(WireFormat, TruncatesLongNameAtUtf8Boundary) {
    const cv::Mat image(1, 1, CV_8UC1, cv::Scalar(0));
    // The two-byte "é" straddles the 1024-byte limit, so it must be dropped whole.
    const std::string name = std::string(1023, 'a') + "é";

    EXPECT_EQ(roundTrip(image, name).name, std::string(1023, 'a'));
}

TEST(WireFormat, PreservesNonAsciiName) {
    const cv::Mat image(1, 1, CV_8UC1, cv::Scalar(0));

    EXPECT_EQ(roundTrip(image, "môj obrázok č.1").name, "môj obrázok č.1");
}

TEST(WireFormat, ReplacesInvalidUtf8InName) {
    const cv::Mat image(1, 1, CV_8UC1, cv::Scalar(0));

    // "café" in Latin-1, as a file name from a Windows ANSI code page arrives; é becomes U+FFFD.
    EXPECT_EQ(roundTrip(image, "caf\xE9").name, "caf\xEF\xBF\xBD");
}

// The viewer applies the same rule to names typed in a rename, so the rule is public.
TEST(WireFormat, SanitiseNameCutsAtCharacterBoundary) {
    const std::string name = std::string(1023, 'a') + "é"; // é is two bytes: 1025 in total
    EXPECT_EQ(visual_debug::sanitiseName(name), std::string(1023, 'a'));
}

TEST(WireFormat, SanitiseNameReplacesInvalidUtf8) {
    EXPECT_EQ(visual_debug::sanitiseName("a\xff"
                                         "b"),
              "a\xEF\xBF\xBD"
              "b");
}

TEST(WireFormat, SanitiseNameKeepsShortValidName) {
    EXPECT_EQ(visual_debug::sanitiseName("Sobel X"), "Sobel X");
}

// --- Encoder input checks ----------------------------------------------------------------------------------

TEST(WireFormat, RejectsUnsupportedImagesOnEncode) {
    unsigned char smallBuffer[16] = {};
    const int threeDimensions[] = {2, 2, 2};
    const std::vector<std::pair<std::string, cv::Mat>> images = {
        {"empty", cv::Mat()},
        {"16-bit float", cv::Mat(2, 2, CV_16FC1)},
        {"five channels", cv::Mat(2, 2, CV_8UC(5))},
        {"three dimensions", cv::Mat(3, threeDimensions, CV_8UC1)},
        // Header-only Mats over a small buffer: the size checks must run before any pixel is read.
        {"too many rows", cv::Mat(32769, 1, CV_8UC1, smallBuffer)},
        {"over 512 MiB", cv::Mat(16384, 16384, CV_64FC2, smallBuffer)},
    };

    for (const auto& [label, image] : images) {
        EXPECT_FALSE(visual_debug::isSupportedImage(image)) << label;
        EXPECT_TRUE(std::holds_alternative<EncodeError>(visual_debug::encode(image, "x"))) << label;
    }
}

// --- Decoder -----------------------------------------------------------------------------------------------

TEST(WireFormat, IgnoresUnknownHeaderFields) {
    const cv::Mat image(2, 3, CV_8UC1, cv::Scalar(9));
    const auto encoded = encodeOrFail(image, "x");
    auto header = nlohmann::json::parse(encoded.header);
    header["timestamp"] = 1;

    const auto decoded = visual_debug::decode(header.dump(), encoded.pixelBytes());

    EXPECT_TRUE(std::holds_alternative<ImageMessage>(decoded));
}

TEST(WireFormat, DecodedImageOwnsItsPixels) {
    const cv::Mat image(2, 3, CV_8UC1, cv::Scalar(9));
    const auto encoded = encodeOrFail(image, "x");
    std::vector<std::byte> buffer(encoded.pixelBytes().begin(), encoded.pixelBytes().end());

    const auto decoded = visual_debug::decode(encoded.header, buffer);
    std::fill(buffer.begin(), buffer.end(), std::byte{0});

    ASSERT_TRUE(std::holds_alternative<ImageMessage>(decoded));
    EXPECT_EQ(cv::norm(std::get<ImageMessage>(decoded).image, image, cv::NORM_INF), 0.0);
}

TEST(WireFormat, RejectsDeeplyNestedHeaderOnSecondaryThread) {
    // Secondary threads have small stacks (512 KiB on macOS). Copying or dumping this document recurses
    // once per level and would overflow; decode may only parse, inspect and destroy it.
    const std::string header = std::string(32000, '[') + std::string(32000, ']');
    ASSERT_LE(header.size(), visual_debug::kMaxHeaderBytes);

    bool rejected = false;
    std::thread worker(
        [&] { rejected = std::holds_alternative<DecodeError>(visual_debug::decode(header, {})); });
    worker.join();

    EXPECT_TRUE(rejected);
}

// Every rejection case below starts from this header and pixel count, changing one thing, and sizes its
// pixel frame as if that one change were accepted, so each case can only fail on the rule it names.
nlohmann::ordered_json validHeader() {
    return {{"protocol", "visual_debug"},
            {"version", 2},
            {"type", "image"},
            {"name", "test"},
            {"rows", 2},
            {"cols", 3},
            {"depth", "8U"},
            {"channels", 1}};
}
constexpr std::size_t kValidPixelBytes = 2 * 3;

TEST(WireFormat, AcceptsTheValidBaseHeader) {
    const std::vector<std::byte> pixels(kValidPixelBytes);

    EXPECT_TRUE(std::holds_alternative<ImageMessage>(visual_debug::decode(validHeader().dump(), pixels)));
}

TEST(WireFormat, AcceptsSentAtOnAnyImage) {
    const std::vector<std::byte> pixels(kValidPixelBytes);
    auto header = validHeader();
    header["sentAt"] = 5;

    const auto decoded = visual_debug::decode(header.dump(), pixels);

    ASSERT_TRUE(std::holds_alternative<ImageMessage>(decoded));
    EXPECT_FALSE(std::get<ImageMessage>(decoded).live);
    EXPECT_EQ(std::get<ImageMessage>(decoded).sentAt, SentTime{5ms});
}

TEST(WireFormat, AcceptsSentAtLimits) {
    const std::vector<std::byte> pixels(kValidPixelBytes);

    for (const std::int64_t milliseconds : {std::int64_t{0}, visual_debug::kMaxSentAtMilliseconds}) {
        auto header = validHeader();
        header["live"] = true;
        header["sentAt"] = milliseconds;

        const auto decoded = visual_debug::decode(header.dump(), pixels);

        ASSERT_TRUE(std::holds_alternative<ImageMessage>(decoded)) << milliseconds;
        EXPECT_EQ(std::get<ImageMessage>(decoded).sentAt, SentTime{std::chrono::milliseconds(milliseconds)});
    }
}

struct RejectionCase {
    std::string label;
    std::string header;
    std::size_t pixelBytes;
    std::string expectedReason;
};

void PrintTo(const RejectionCase& rejectionCase, std::ostream* out) { *out << rejectionCase.label; }

RejectionCase withField(std::string label, const std::string& key, nlohmann::ordered_json value,
                        std::string expectedReason, std::size_t pixelBytes = kValidPixelBytes) {
    auto header = validHeader();
    header[key] = std::move(value);
    return {std::move(label), header.dump(), pixelBytes, std::move(expectedReason)};
}

RejectionCase withoutField(std::string label, const std::string& key, std::string expectedReason) {
    auto header = validHeader();
    header.erase(key);
    return {std::move(label), header.dump(), kValidPixelBytes, std::move(expectedReason)};
}

std::vector<RejectionCase> rejectionCases() {
    std::string invalidUtf8 = validHeader().dump();
    invalidUtf8.replace(invalidUtf8.find("test"), 4, "\xFF");

    auto oversized = validHeader();
    oversized["padding"] = std::string(visual_debug::kMaxHeaderBytes, ' ');

    auto huge = validHeader();
    huge["rows"] = 32768;
    huge["cols"] = 32768;
    huge["depth"] = "64F";
    huge["channels"] = 4;

    const std::string kRows = "rows out of range";
    const std::string kVersion = "unsupported version";
    const std::string kChannels = "channels out of range";
    const std::string kLive = "live is not a boolean";
    const std::string kSentAtRange = "sentAt out of range";
    return {
        {"NotJson", "not json", kValidPixelBytes, "header is not valid JSON"},
        {"InvalidUtf8", invalidUtf8, kValidPixelBytes, "header is not valid JSON"},
        {"JsonArray", "[1, 2, 3]", kValidPixelBytes, "header is not a JSON object"},
        {"HeaderTooLarge", oversized.dump(), kValidPixelBytes, "header too large"},
        withField("WrongProtocol", "protocol", "other", "unknown protocol"),
        withField("OldVersion", "version", 1, kVersion),
        withField("VersionAsString", "version", "2", kVersion),
        withField("VersionWrappingTo2", "version", std::int64_t{4294967298}, kVersion),
        withField("WrongType", "type", "video", "unsupported message type"),
        withoutField("NameMissing", "name", "name is missing or not a string"),
        withField("NameTooLong", "name", std::string(1025, 'n'), "name too long"),
        withField("ZeroRows", "rows", 0, kRows),
        withField("RowsAsBoolean", "rows", true, kRows),
        withField("RowsAsFloat", "rows", 2.0, kRows),
        withField("RowsWrappingTo1", "rows", std::int64_t{4294967297}, kRows, 1 * 3),
        withField("NegativeRows", "rows", -1, kRows),
        withField("TooManyCols", "cols", 32769, "cols out of range", 2 * 32769),
        withField("HalfFloatDepth", "depth", "16F", "unsupported depth", 2 * 3 * 2),
        withField("FiveChannels", "channels", 5, kChannels, 2 * 3 * 5),
        withField("FractionalChannels", "channels", 1.5, kChannels),
        withField("LiveAsNumber", "live", 1, kLive),
        withField("LiveAsString", "live", "true", kLive),
        withField("LiveAsNull", "live", nullptr, kLive),
        withField("NegativeSentAt", "sentAt", -1, kSentAtRange),
        withField("SentAtAfterYear9999", "sentAt", std::int64_t{253402300800000}, kSentAtRange),
        withField("FractionalSentAt", "sentAt", 1.5, kSentAtRange),
        withField("SentAtAsString", "sentAt", "5", kSentAtRange),
        withField("SentAtAsBoolean", "sentAt", true, kSentAtRange),
        {"PixelsOneByteShort", validHeader().dump(), kValidPixelBytes - 1, "pixel frame has the wrong size"},
        {"PixelsOneByteLong", validHeader().dump(), kValidPixelBytes + 1, "pixel frame has the wrong size"},
        {"ImageOver512MiB", huge.dump(), 0, "image too large"},
    };
}

class WireFormatRejection : public testing::TestWithParam<RejectionCase> {};

TEST_P(WireFormatRejection, RejectsMalformedMessage) {
    const auto& rejectionCase = GetParam();
    const std::vector<std::byte> pixels(rejectionCase.pixelBytes);

    const auto result = visual_debug::decode(rejectionCase.header, pixels);

    ASSERT_TRUE(std::holds_alternative<DecodeError>(result));
    const auto& reason = std::get<DecodeError>(result).reason;
    EXPECT_EQ(reason, rejectionCase.expectedReason);
    EXPECT_TRUE(isAscii(reason)) << reason;
}

INSTANTIATE_TEST_SUITE_P(EveryRule, WireFormatRejection, testing::ValuesIn(rejectionCases()),
                         [](const testing::TestParamInfo<RejectionCase>& info) { return info.param.label; });

} // namespace
