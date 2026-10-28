#pragma once

#include <string>
#include <string_view>

namespace visual_debug {

inline constexpr std::string_view kDefaultEndpoint = "tcp://127.0.0.1:7139";
inline constexpr char kEndpointEnvironmentVariable[] = "VISUAL_DEBUG_ENDPOINT";

// VISUAL_DEBUG_ENDPOINT if it is set and not empty, otherwise kDefaultEndpoint. The sender and the viewer
// both start from this, so they agree without any configuration. The only place the environment is read.
std::string defaultEndpoint();

// True for literal loopback TCP addresses and for same-machine transports (ipc, inproc). Host names,
// including "localhost", are not trusted: a hosts file can point them anywhere.
bool isLoopbackEndpoint(std::string_view endpoint);

} // namespace visual_debug
