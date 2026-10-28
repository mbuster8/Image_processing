#pragma once

#include <string>
#include <string_view>

namespace viewer_core {

// The name to suggest when saving an image, without the extension: an image named after its file
// ("photo.webp", or "photo.webp #3" from the demo client) would otherwise be offered as "photo.webp.png".
// Only the extensions of formats the viewer can open are dropped, so "result v1.2" keeps its dot, and a name
// that is nothing but an extension is kept whole. The caller adds ".png".
std::string saveNameStem(std::string_view imageName);

} // namespace viewer_core
