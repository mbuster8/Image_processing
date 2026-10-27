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

// Reasons are fixed ASCII texts and never contain request bytes, so building the reply cannot fail on
// hostile input.
std::string errorReply(std::string_view reason) {
    return nlohmann::json{{"status", "error"}, {"reason", reason}}.dump();
}

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
            return StartError{"refusing to listen on " + m_options.endpoint +
                              ": not a loopback address, and remote connections are not allowed"};
        }

        zmq::socket_t socket(m_context, zmq::socket_type::rep);
        socket.set(zmq::sockopt::linger, 0);
        socket.set(zmq::sockopt::maxmsgsize, static_cast<std::int64_t>(kMaxPixelBytes));
        socket.set(zmq::sockopt::rcvhwm, kReceiveQueueLength);
        // Only an IPv6 address needs IPv6 on: with it on, a 127.0.0.1 bind would report its endpoint as the
        // IPv4-mapped tcp://[::ffff:127.0.0.1]:port, which is neither what was asked for nor a loopback literal.
        if (m_options.endpoint.find('[') != std::string::npos) {
            socket.set(zmq::sockopt::ipv6, true);
        }
        try {
            socket.bind(m_options.endpoint);
        } catch (const zmq::error_t& error) {
            return StartError{"could not listen on " + m_options.endpoint + ": " + error.what()};
        }
        m_boundEndpoint = socket.get(zmq::sockopt::last_endpoint);

        m_stopRequested = false;
        m_worker = std::thread([this, socket = std::move(socket)]() mutable { run(socket); });
        return std::nullopt;
    }

    void stop() {
        m_stopRequested = true;
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    bool isRunning() const { return m_worker.joinable(); }

    std::string boundEndpoint() const { return m_boundEndpoint; }

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
                socket.send(zmq::buffer(reply), zmq::send_flags::none);
                replyOwed = false;
            } catch (...) {
                // One bad message, or a failing sink, never stops the server. A REP socket must answer
                // every request before it can receive the next.
                if (replyOwed) {
                    try {
                        socket.send(zmq::buffer(errorReply("internal error")), zmq::send_flags::none);
                    } catch (...) {
                    }
                }
            }
        }
    }

    std::string handle(std::vector<zmq::message_t>& frames) {
        if (frames.size() != 2) {
            return errorReply("expected exactly two frames");
        }
        const auto& pixels = frames[1];
        auto decoded = decode(frames[0].to_string_view(), {pixels.data<std::byte>(), pixels.size()});
        if (const auto* error = std::get_if<DecodeError>(&decoded)) {
            return errorReply(error->reason);
        }
        if (!m_sink.onImageReceived(std::get<ImageMessage>(std::move(decoded)))) {
            return errorReply("viewer busy");
        }
        return std::string(kOkReply);
    }

    ImageSink& m_sink;
    ServerOptions m_options;
    zmq::context_t m_context;
    std::thread m_worker; // owns the socket; joined before the context is destroyed
    std::atomic<bool> m_stopRequested{false};
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

} // namespace visual_debug
