#include "visual_debug/endpoint.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace visual_debug {

std::string defaultEndpoint() {
#if defined(_MSC_VER)
#pragma warning(suppress : 4996) // getenv is fine here: nothing in this library modifies the environment
#endif
    const char* value = std::getenv(kEndpointEnvironmentVariable);
    if (value == nullptr || *value == '\0') {
        return std::string(kDefaultEndpoint);
    }
    return value;
}

bool isLoopbackEndpoint(std::string_view endpoint) {
    constexpr std::array<std::string_view, 4> kLoopbackPrefixes{"tcp://127.0.0.1:", "tcp://[::1]:", "ipc://",
                                                                "inproc://"};
    return std::any_of(kLoopbackPrefixes.begin(), kLoopbackPrefixes.end(),
                       [&](std::string_view prefix) { return endpoint.starts_with(prefix); });
}

} // namespace visual_debug
