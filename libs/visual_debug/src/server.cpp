#include "visual_debug/server.h"

#include "visual_debug/wire_format.h"

#include <nlohmann/json.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include <atomic>
#include <cstdint>
#include <iterator>
#include <thread>
#include <variant>
#include <vector>

namespace visual_debug {
namespace {

// ZeroMQ holds a whole multipart message before delivering it and has no frame-count limit, so the
// queue length bounds how much a peer can make the viewer buffer (see docs/wire-format.md).
constexpr int kReceiveQueueLength = 4;

constexpr std::string_view kOkReply = R"({"status":"ok"})";

constexpr std::string_view kInternalError = "internal error";

// Reasons are fixed ASCII texts and never contain request bytes, so building the reply cannot fail on
// hostile input.
std::string errorReply(std::string_view reason) {
    return nlohmann::json{{"status", "error"}, {"reason", reason}}.dump();
}

// The reply to send, and for an error reply its reason, which the sink is told about.
struct Reply {
    std::string text;
    std::string rejectedReason; // empty when the image was taken
};

Reply rejected(std::string_view reason) { return {errorReply(reason), std::string(reason)}; }

} // namespace

class Server::Impl {
public:
    Impl(ImageSink& sink, ServerOptions options) : m_sink(sink), m_options(std::move(options)) {}

    ~Impl() { stop(); }

    std::optional<StartError> start() {
        if (m_worker.joinable()) {
            return StartError{"already running"};
        }
        if (!m_options.allowRemote && !isLoopbackEndpoint(m_options.endpoint)) {
            return StartError{"refusing to listen on [" + m_options.endpoint +
                              "]: not a loopback address, and remote connections are not allowed"};
        }

        // A context per start, so that stop() can terminate it: see stop().
        m_context.emplace();
        zmq::socket_t socket(*m_context, zmq::socket_type::rep);
        socket.set(zmq::sockopt::linger, 0);
        socket.set(zmq::sockopt::maxmsgsize, static_cast<std::int64_t>(kMaxPixelBytes));
        socket.set(zmq::sockopt::rcvhwm, kReceiveQueueLength);
        // Only an IPv6 address needs IPv6 on: with it on, a 127.0.0.1 bind would report its endpoint as the
        // IPv4-mapped tcp://[::ffff:127.0.0.1]:port, which is neither what was asked for nor a loopback
        // literal.
        if (m_options.endpoint.find('[') != std::string::npos) {
            socket.set(zmq::sockopt::ipv6, true);
        }
        try {
            socket.bind(m_options.endpoint);
        } catch (const zmq::error_t& error) {
            return StartError{"could not listen on [" + m_options.endpoint + "]: " + error.what()};
        }
        m_boundEndpoint = socket.get(zmq::sockopt::last_endpoint);

        m_stopRequested = false;
        m_worker = std::thread([this, socket = std::move(socket)]() mutable { run(socket); });
        return std::nullopt;
    }

    // ZeroMQ closes a socket on its own background thread, so the port is still taken when the worker has
    // returned. Terminating the context waits for that, so the port is free when stop() returns and an
    // immediate start() can bind it again.
    void stop() {
        m_stopRequested = true;
        if (m_worker.joinable()) {
            m_worker.join();
        }
        m_context.reset();
    }

    bool isRunning() const { return m_worker.joinable(); }

    std::string boundEndpoint() const { return m_boundEndpoint; }

    void setPaused(bool paused) { m_paused = paused; }

    bool isPaused() const { return m_paused; }

private:
    void run(zmq::socket_t& socket) {
        while (!m_stopRequested) {
            bool replyOwed = false;
            try {
                zmq::pollitem_t item{socket.handle(), 0, ZMQ_POLLIN, 0};
                zmq::poll(&item, 1, m_options.pollInterval);
                if ((item.revents & ZMQ_POLLIN) == 0) {
                    continue;
                }
                std::vector<zmq::message_t> frames;
                if (!zmq::recv_multipart(socket, std::back_inserter(frames), zmq::recv_flags::dontwait)) {
                    continue;
                }
                replyOwed = true;
                const auto reply = handle(frames);
                if (!reply.rejectedReason.empty()) {
                    m_sink.onRequestRejected(reply.rejectedReason);
                }
                socket.send(zmq::buffer(reply.text), zmq::send_flags::none);
                replyOwed = false;
            } catch (...) {
                // One bad message, or a failing sink, never stops the server. A REP socket must answer
                // every request before it can receive the next.
                if (replyOwed) {
                    try {
                        m_sink.onRequestRejected(kInternalError);
                    } catch (...) {
                    }
                    try {
                        socket.send(zmq::buffer(errorReply(kInternalError)), zmq::send_flags::none);
                    } catch (...) {
                    }
                }
            }
        }
    }

    Reply handle(std::vector<zmq::message_t>& frames) {
        // Checked first, so a paused viewer spends nothing on a request, however malformed.
        if (m_paused) {
            return rejected("viewer paused");
        }
        if (frames.size() != 2) {
            return rejected("expected exactly two frames");
        }
        const auto& pixels = frames[1];
        auto decoded = decode(frames[0].to_string_view(), {pixels.data<std::byte>(), pixels.size()});
        if (const auto* error = std::get_if<DecodeError>(&decoded)) {
            return rejected(error->reason);
        }
        auto message = std::get<ImageMessage>(std::move(decoded));
        message.receivedAt = std::chrono::system_clock::now();
        if (!m_sink.onImageReceived(std::move(message))) {
            return rejected("viewer busy");
        }
        return {std::string(kOkReply), {}};
    }

    ImageSink& m_sink;
    ServerOptions m_options;
    std::optional<zmq::context_t> m_context;
    std::thread m_worker; // owns the socket; joined before the context is destroyed
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_paused{false};
    std::string m_boundEndpoint;
};

Server::Server(ImageSink& sink, ServerOptions options)
    : m_impl(std::make_unique<Impl>(sink, std::move(options))) {}

Server::~Server() = default;

std::optional<StartError> Server::start() {
    try {
        return m_impl->start();
    } catch (const std::exception& error) {
        return StartError{error.what()};
    }
}

void Server::stop() { m_impl->stop(); }

bool Server::isRunning() const { return m_impl->isRunning(); }

std::string Server::boundEndpoint() const { return m_impl->boundEndpoint(); }

void Server::setPaused(bool paused) { m_impl->setPaused(paused); }

bool Server::isPaused() const { return m_impl->isPaused(); }

} // namespace visual_debug
