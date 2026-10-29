#include "visual_debug/client.h"

#include "visual_debug/wire_format.h"

#include <nlohmann/json.hpp>
#include <zmq.hpp>

#include <optional>
#include <variant>

namespace visual_debug {
namespace {

using Clock = std::chrono::steady_clock;

// Whatever answers on the viewer's port cannot push a large reply into the host application.
constexpr std::int64_t kMaxReplyBytes = 64 * 1024;

bool isOkReply(std::string_view text) {
    const auto reply = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (!reply.is_object()) {
        return false;
    }
    const auto status = reply.find("status");
    return status != reply.end() && status->is_string() && status->get_ref<const std::string&>() == "ok";
}

} // namespace

class Client::Impl {
public:
    explicit Impl(ClientOptions options) : m_options(std::move(options)) {}

    SendResult send(const cv::Mat& image, std::string_view name) {
        // Cheap checks first: while no viewer is available, an image is never encoded, so a region of
        // interest is not copied on every frame.
        if (!isSupportedImage(image)) {
            return SendResult::InvalidImage;
        }
        if (Clock::now() < m_backOffUntil) {
            return SendResult::ViewerUnavailable;
        }
        if (!m_socket && !openSocket()) {
            return SendResult::InvalidEndpoint;
        }

        // Only a fresh socket waits for its connection. Afterwards ZeroMQ keeps reconnecting in the
        // background, so a host application without a viewer is blocked once, not on every retry.
        const auto connectWait = m_socketIsFresh ? m_options.replyTimeout : std::chrono::milliseconds(0);
        m_socketIsFresh = false;
        if (!waitFor(ZMQ_POLLOUT, connectWait)) {
            startBackOff();
            return SendResult::ViewerUnavailable;
        }

        const auto encoded = encode(image, name);
        if (std::holds_alternative<EncodeError>(encoded)) {
            return SendResult::InvalidImage;
        }
        const auto& message = std::get<EncodedMessage>(encoded);
        const auto pixels = message.pixelBytes();
        // message_t copies the pixels, so the caller may change its Mat as soon as send() returns.
        if (!m_socket->send(zmq::buffer(message.header),
                            zmq::send_flags::sndmore | zmq::send_flags::dontwait) ||
            !m_socket->send(zmq::message_t(pixels.data(), pixels.size()), zmq::send_flags::dontwait)) {
            recover();
            return SendResult::ViewerUnavailable;
        }

        zmq::message_t reply;
        if (!waitFor(ZMQ_POLLIN, replyDeadline(pixels.size())) ||
            !m_socket->recv(reply, zmq::recv_flags::dontwait)) {
            // A REQ socket still waiting for a lost reply cannot send again, so start afresh ("Lazy Pirate").
            recover();
            return SendResult::Timeout;
        }
        return isOkReply(reply.to_string_view()) ? SendResult::Delivered : SendResult::Rejected;
    }

    // After an unexpected error the socket's state is unknown: drop it and back off. Never throws.
    void recover() noexcept {
        m_socket.reset();
        startBackOff();
    }

private:
    bool openSocket() {
        zmq::socket_t socket(m_context, zmq::socket_type::req);
        socket.set(zmq::sockopt::linger, 0);       // process exit never waits for undelivered images
        socket.set(zmq::sockopt::immediate, true); // no connection means "unavailable", not "queued"
        socket.set(zmq::sockopt::maxmsgsize, kMaxReplyBytes);
        socket.set(zmq::sockopt::ipv6, true); // dual-stack: tcp://[::1] works as well as 127.0.0.1
        try {
            socket.connect(m_options.endpoint);
        } catch (const zmq::error_t&) {
            return false;
        }
        m_socket = std::move(socket);
        m_socketIsFresh = true;
        return true;
    }

    bool waitFor(short events, std::chrono::milliseconds timeout) {
        zmq::pollitem_t item{m_socket->handle(), 0, events, 0};
        zmq::poll(&item, 1, timeout);
        return (item.revents & events) != 0;
    }

    // Copying a large image through loopback takes a while (48 MiB measured 140-360 ms), so the deadline
    // grows with the image instead of misreporting a slow delivery as Timeout.
    std::chrono::milliseconds replyDeadline(std::size_t pixelBytes) const {
        constexpr std::size_t kMebibyte = 1024 * 1024;
        const auto mebibytes =
            static_cast<std::chrono::milliseconds::rep>((pixelBytes + kMebibyte - 1) / kMebibyte);
        return m_options.replyTimeout + std::chrono::milliseconds(10 * mebibytes);
    }

    void startBackOff() noexcept { m_backOffUntil = Clock::now() + m_options.retryInterval; }

    ClientOptions m_options;
    zmq::context_t m_context;
    std::optional<zmq::socket_t> m_socket; // declared after the context, so it closes first
    bool m_socketIsFresh = false;
    Clock::time_point m_backOffUntil{};
};

Client::Client(ClientOptions options) : m_impl(std::make_unique<Impl>(std::move(options))) {}

Client::~Client() = default;

SendResult Client::send(const cv::Mat& image, std::string_view name) noexcept {
    try {
        return m_impl->send(image, name);
    } catch (...) {
        // A debugging aid must never take the host application down: ZeroMQ errors, a malformed reply or
        // running out of memory while copying a large image all end here.
        m_impl->recover();
        return SendResult::ViewerUnavailable;
    }
}

} // namespace visual_debug
