#pragma once

#include "visual_debug/endpoint.h"
#include "visual_debug/image_sink.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace visual_debug {

struct ServerOptions {
    std::string endpoint{kDefaultEndpoint};
    // Accept an endpoint other machines can reach. The channel is neither encrypted nor authenticated.
    bool allowRemote = false;
    // How often the receive thread checks for a stop request.
    std::chrono::milliseconds pollInterval{100};
};

struct StartError {
    std::string reason;
};

// Receives images on its own thread, decodes and validates them, and offers each one to an ImageSink,
// which is called on that thread. One malformed message never stops it: the sender gets an error reply.
class Server {
public:
    explicit Server(ImageSink& sink, ServerOptions options = {});
    ~Server(); // stops the receive thread

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Binds and starts the receive thread; a failure (port in use, refused endpoint) is returned, not thrown.
    // After stop() it binds again.
    [[nodiscard]] std::optional<StartError> start();
    void stop(); // idempotent
    bool isRunning() const;
    // While paused the server keeps its port and answers every request with the error "viewer paused",
    // without decoding it or offering it to the sink. Safe to call from any thread.
    void setPaused(bool paused);
    bool isPaused() const;
    // The endpoint actually bound, with the port filled in when the options asked for "*".
    std::string boundEndpoint() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace visual_debug
