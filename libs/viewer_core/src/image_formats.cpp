#include "viewer_core/image_formats.h"

#include <array>

namespace viewer_core {
namespace {

constexpr std::array<std::string_view, 22> kImageExtensions{
    "png", "jpg", "jpeg", "jpe", "bmp", "dib", "tif", "tiff", "webp", "pbm", "pgm",
    "ppm", "pxm", "pnm",  "pfm", "exr", "hdr", "pic", "sr",   "ras",  "jp2", "avif"};

} // namespace

std::span<const std::string_view> imageExtensions() { return kImageExtensions; }

} // namespace viewer_core
