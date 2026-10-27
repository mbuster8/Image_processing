#include "visual_debug/send_image.h"

#include "visual_debug/client.h"
#include "visual_debug/endpoint.h"

#include <mutex>

namespace visual_debug {

SendResult sendImage(const cv::Mat& image, std::string_view name) noexcept {
    static std::mutex mutex;
    const std::lock_guard lock(mutex);
    try {
        // Created on first use, so a program that never calls sendImage never opens a socket.
        static Client client(ClientOptions{.endpoint = defaultEndpoint()});
        return client.send(image, name);
    } catch (...) {
        // Only creating the client can throw; the next call tries again.
        return SendResult::ViewerUnavailable;
    }
}

} // namespace visual_debug
