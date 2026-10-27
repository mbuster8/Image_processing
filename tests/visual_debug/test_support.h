#pragma once

#include "visual_debug/image_sink.h"
#include "visual_debug/server.h"

#include <nlohmann/json.hpp>
#include <zmq.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace test_support {

// Thread-safe sink that keeps every image it receives.
class CollectingSink : public visual_debug::ImageSink {
public:
    bool onImageReceived(visual_debug::ImageMessage message) override {
        {
            std::lock_guard lock(m_mutex);
            m_messages.push_back(std::move(message));
        }
        m_changed.notify_all();
        return true;
    }

    // False if fewer than `count` images have arrived when the timeout expires.
    bool waitForCount(std::size_t count, std::chrono::milliseconds timeout) {
        std::unique_lock lock(m_mutex);
        return m_changed.wait_for(lock, timeout, [&] { return m_messages.size() >= count; });
    }

    std::vector<visual_debug::ImageMessage> messages() const {
        std::lock_guard lock(m_mutex);
        return m_messages;
    }

    std::size_t count() const {
        std::lock_guard lock(m_mutex);
        return m_messages.size();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    std::vector<visual_debug::ImageMessage> m_messages;
};

// A viewer that is always too busy to take another image.
class BusySink : public visual_debug::ImageSink {
public:
    bool onImageReceived(visual_debug::ImageMessage) override { return false; }
};

inline void setEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

inline void unsetEnvironment(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

inline bool isAscii(std::string_view text) {
    return std::all_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
}

// A loopback endpoint that was free a moment ago and has nothing listening on it now.
inline std::string unusedEndpoint() {
    CollectingSink sink;
    visual_debug::Server server(sink, {.endpoint = "tcp://127.0.0.1:*"});
    if (server.start()) {
        return {};
    }
    auto endpoint = server.boundEndpoint();
    server.stop();
    return endpoint;
}

// Sends raw frames from a plain REQ socket, as a misbehaving sender would, and returns the parsed reply,
// or nothing if no reply arrives within a second.
inline std::optional<nlohmann::json> rawRequest(const std::string& endpoint, const std::vector<std::string>& frames) {
    zmq::context_t context;
    zmq::socket_t socket(context, zmq::socket_type::req);
    socket.set(zmq::sockopt::linger, 0);
    socket.connect(endpoint);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto flags = i + 1 < frames.size() ? zmq::send_flags::sndmore : zmq::send_flags::none;
        if (!socket.send(zmq::buffer(frames[i]), flags)) {
            return std::nullopt;
        }
    }
    zmq::pollitem_t item{socket.handle(), 0, ZMQ_POLLIN, 0};
    zmq::poll(&item, 1, std::chrono::milliseconds(1000));
    if ((item.revents & ZMQ_POLLIN) == 0) {
        return std::nullopt;
    }
    zmq::message_t reply;
    if (!socket.recv(reply)) {
        return std::nullopt;
    }
    return nlohmann::json::parse(reply.to_string_view(), nullptr, /*allow_exceptions=*/false);
}

} // namespace test_support
