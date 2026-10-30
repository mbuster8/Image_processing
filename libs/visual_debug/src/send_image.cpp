#include "visual_debug/send_image.h"

#include "visual_debug/client.h"
#include "visual_debug/endpoint.h"

#include <mutex>

namespace visual_debug {
namespace {

// sendImage and sendLive share one Client, created on first use, so a program that never calls either never
// opens a socket. The statics live in plain functions: a template's would be one per instantiation.
std::mutex& sharedClientMutex() {
    static std::mutex mutex;
    return mutex;
}

Client& sharedClient() {
    static Client client(ClientOptions{.endpoint = defaultEndpoint()});
    return client;
}

// Calls are serialised: a Client is not thread-safe.
template <typename Send> SendResult sendThroughSharedClient(Send send) noexcept {
    const std::lock_guard lock(sharedClientMutex());
    try {
        return send(sharedClient());
    } catch (...) {
        // Only creating the client can throw; the next call tries again.
        return SendResult::ViewerUnavailable;
    }
}

} // namespace

SendResult sendImage(const cv::Mat& image, std::string_view name) noexcept {
    return sendThroughSharedClient([&](Client& client) { return client.send(image, name); });
}

SendResult sendLive(const cv::Mat& frame, std::string_view channel) noexcept {
    return sendThroughSharedClient([&](Client& client) { return client.sendLive(frame, channel); });
}

} // namespace visual_debug
