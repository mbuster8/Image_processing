#pragma once

#include "visual_debug/endpoint.h"
#include "visual_debug/image_sender.h"

#include <chrono>
#include <memory>
#include <string>

namespace visual_debug {

struct ClientOptions {
    std::string endpoint{kDefaultEndpoint};
    // Longest wait for a reply to a small image; each MiB of pixels adds 10 ms.
    std::chrono::milliseconds replyTimeout{500};
    // After a failure, sends return ViewerUnavailable immediately for this long.
    std::chrono::milliseconds retryInterval{1000};
};

// Sends images to a viewer over ZeroMQ REQ/REP. With no viewer running, the host application is blocked
// once for replyTimeout, not on every send; once a viewer appears, sending recovers by itself.
// Owns its socket for its lifetime. Not thread-safe: use one Client per thread, or sendImage().
class Client final : public ImageSender {
public:
    explicit Client(ClientOptions options = {});
    ~Client() override;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    SendResult send(const cv::Mat& image, std::string_view name) noexcept override;
    // Marks the frame live and stamps it with this machine's clock (sentAt), just before encoding it.
    SendResult sendLive(const cv::Mat& frame, std::string_view channel) noexcept override;

private:
    SendResult sendGuarded(const cv::Mat& image, std::string_view name, bool live) noexcept;

    class Impl; // keeps ZeroMQ out of host applications' headers
    std::unique_ptr<Impl> m_impl;
};

} // namespace visual_debug
