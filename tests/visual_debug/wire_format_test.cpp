#include "visual_debug/wire_format.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace {

using visual_debug::DecodeError;
using visual_debug::EncodedMessage;
using visual_debug::EncodeError;
using visual_debug::ImageMessage;

EncodedMessage encodeOrFail(const cv::Mat& image, std::string_view name) {
    auto result = visual_debug::encode(image, name);
    if (const auto* error = std::get_if<EncodeError>(&result)) {
        ADD_FAILURE() << "encode failed: " << error->reason;
        return {};
    }
    return std::get<EncodedMessage>(std::move(result));
}

ImageMessage roundTrip(const cv::Mat& image, std::string_view name) {
    const auto encoded = encodeOrFail(image, name);
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

struct RejectionCase {
    std::string label;
    std::string header;
    std::size_t pixelBytes;
};

void PrintTo(const RejectionCase& rejectionCase, std::ostream* out) { *out << rejectionCase.label; }

RejectionCase withField(std::string label, const std::string& key, nlohmann::ordered_json value,
                        std::size_t pixelBytes = kValidPixelBytes) {
    auto header = validHeader();
    header[key] = std::move(value);
    return {std::move(label), header.dump(), pixelBytes};
}

RejectionCase withoutField(std::string label, const std::string& key) {
    auto header = validHeader();
    header.erase(key);
    return {std::move(label), header.dump(), kValidPixelBytes};
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

    return {
        {"NotJson", "not json", kValidPixelBytes},
        {"InvalidUtf8", invalidUtf8, kValidPixelBytes},
        {"JsonArray", "[1, 2, 3]", kValidPixelBytes},
        {"HeaderTooLarge", oversized.dump(), kValidPixelBytes},
        withField("WrongProtocol", "protocol", "other"),
        withField("OldVersion", "version", 1),
        withField("VersionAsString", "version", "2"),
        withField("VersionWrappingTo2", "version", std::int64_t{4294967298}),
        withField("WrongType", "type", "video"),
        withoutField("NameMissing", "name"),
        withField("NameTooLong", "name", std::string(1025, 'n')),
        withField("ZeroRows", "rows", 0),
        withField("RowsAsBoolean", "rows", true),
        withField("RowsAsFloat", "rows", 2.0),
        withField("RowsWrappingTo1", "rows", std::int64_t{4294967297}, 1 * 3),
        withField("NegativeRows", "rows", -1),
        withField("TooManyCols", "cols", 32769, 2 * 32769),
        withField("HalfFloatDepth", "depth", "16F", 2 * 3 * 2),
        withField("FiveChannels", "channels", 5, 2 * 3 * 5),
        withField("FractionalChannels", "channels", 1.5),
        {"PixelsOneByteShort", validHeader().dump(), kValidPixelBytes - 1},
        {"PixelsOneByteLong", validHeader().dump(), kValidPixelBytes + 1},
        {"ImageOver512MiB", huge.dump(), 0},
    };
}

class WireFormatRejection : public testing::TestWithParam<RejectionCase> {};

TEST_P(WireFormatRejection, RejectsMalformedMessage) {
    const auto& rejectionCase = GetParam();
    const std::vector<std::byte> pixels(rejectionCase.pixelBytes);

    const auto result = visual_debug::decode(rejectionCase.header, pixels);

    ASSERT_TRUE(std::holds_alternative<DecodeError>(result));
    const auto& reason = std::get<DecodeError>(result).reason;
    EXPECT_FALSE(reason.empty());
    EXPECT_TRUE(isAscii(reason)) << reason;
}

INSTANTIATE_TEST_SUITE_P(EveryRule, WireFormatRejection, testing::ValuesIn(rejectionCases()),
                         [](const testing::TestParamInfo<RejectionCase>& info) { return info.param.label; });

} // namespace
