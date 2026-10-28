#include "viewer_core/save_name.h"

#include "viewer_core/image_formats.h"

#include <algorithm>
#include <cctype>

namespace viewer_core {
namespace {

// What File → Open accepts.
bool isImageExtension(std::string_view extension) {
    std::string lower(extension);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::ranges::find(imageExtensions(), lower) != imageExtensions().end();
}

// The length of a trailing " #<digits>" send number, or 0.
std::size_t sendNumberLength(std::string_view name) {
    const auto hash = name.rfind(" #");
    if (hash == std::string_view::npos || hash + 2 == name.size()) {
        return 0;
    }
    const auto digits = name.substr(hash + 2);
    const bool allDigits =
        std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    return allDigits ? name.size() - hash : 0;
}

} // namespace

std::string saveNameStem(std::string_view imageName) {
    const auto suffix = imageName.substr(imageName.size() - sendNumberLength(imageName));
    const auto stem = imageName.substr(0, imageName.size() - suffix.size());
    const auto dot = stem.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || !isImageExtension(stem.substr(dot + 1))) {
        return std::string(imageName);
    }
    return std::string(stem.substr(0, dot)) + std::string(suffix);
}

} // namespace viewer_core
