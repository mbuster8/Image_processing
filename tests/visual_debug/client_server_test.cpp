#include "test_support.h"

#include "visual_debug/client.h"
#include "visual_debug/server.h"
#include "visual_debug/wire_format.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <zmq.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <variant>
#include <vector>

// Real ZeroMQ over loopback. Every server binds an ephemeral port ("tcp://127.0.0.1:*") and reports it, so
// no test touches the viewer's port 7139 and tests can run while a viewer is open.
namespace {

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using test_support::BusySink;
using test_support::CollectingSink;
using test_support::unusedEndpoint;
using visual_debug::Client;
using visual_debug::SendResult;
using visual_debug::Server;

constexpr const char* kAnyLoopbackPort = "tcp://127.0.0.1:*";

cv::Mat smallImage() { return cv::Mat(3, 4, CV_8UC3, cv::Scalar(1, 2, 3)); }

// Counts the pixel buffers cv::Mat allocates while installed as the default allocator, so a test can show
// that an operation copied no image.
class CountingAllocator : public cv::MatAllocator {
public:
    cv::UMatData* allocate(int dims, const int* sizes, int type, void* data, size_t* step,
                           cv::AccessFlag flags, cv::UMatUsageFlags usageFlags) const override {
        ++m_count;
        return cv::Mat::getStdAllocator()->allocate(dims, sizes, type, data, step, flags, usageFlags);
    }
    bool allocate(cv::UMatData* data, cv::AccessFlag flags, cv::UMatUsageFlags usageFlags) const override {
        return cv::Mat::getStdAllocator()->allocate(data, flags, usageFlags);
    }
    void deallocate(cv::UMatData* data) const override { cv::Mat::getStdAllocator()->deallocate(data); }

    int count() const { return m_count; }

private:
    mutable std::atomic<int> m_count{0};
};

// Makes an allocator cv::Mat's default for one scope, then restores the previous default, even when a failed
// assertion ends the test early. setDefaultAllocator(nullptr) is no reset: it leaves cv::Mat without an
// allocator, so the next allocation anywhere in the process dereferences null.
class DefaultAllocatorScope {
public:
    explicit DefaultAllocatorScope(cv::MatAllocator* allocator) : m_previous(cv::Mat::getDefaultAllocator()) {
        cv::Mat::setDefaultAllocator(allocator);
    }
    ~DefaultAllocatorScope() { cv::Mat::setDefaultAllocator(m_previous); }
    DefaultAllocatorScope(const DefaultAllocatorScope&) = delete;
    DefaultAllocatorScope& operator=(const DefaultAllocatorScope&) = delete;

private:
    cv::MatAllocator* m_previous;
};

testing::AssertionResult started(Server& server) {
    if (const auto error = server.start()) {
        return testing::AssertionFailure() << error->reason;
    }
    return testing::AssertionSuccess();
}

TEST(ClientServer, DeliversImageToSink) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});
    const cv::Mat image = smallImage();

    EXPECT_EQ(client.send(image, "first"), SendResult::Delivered);

    ASSERT_TRUE(sink.waitForCount(1, 1s));
    const auto received = sink.messages().front();
    EXPECT_EQ(received.name, "first");
    EXPECT_EQ(cv::norm(received.image, image, cv::NORM_INF), 0.0);
}

TEST(ClientServer, ReportsViewerUnavailableWithinReplyTimeout) {
    Client client({.endpoint = unusedEndpoint(), .replyTimeout = 100ms});

    const auto start = Clock::now();
    EXPECT_EQ(client.send(smallImage(), "x"), SendResult::ViewerUnavailable);
    EXPECT_LT(Clock::now() - start, 250ms);
}

TEST(ClientServer, FailsImmediatelyDuringRetryInterval) {
    Client client({.endpoint = unusedEndpoint(), .replyTimeout = 100ms, .retryInterval = 1000ms});
    ASSERT_EQ(client.send(smallImage(), "x"), SendResult::ViewerUnavailable);

    const auto start = Clock::now();
    EXPECT_EQ(client.send(smallImage(), "x"), SendResult::ViewerUnavailable);
    EXPECT_LT(Clock::now() - start, 20ms);
}

TEST(ClientServer, BlocksOnlyOnceWithoutViewer) {
    // A host application sending every frame must not stall once per back-off period.
    Client client({.endpoint = unusedEndpoint(), .replyTimeout = 100ms, .retryInterval = 100ms});

    auto blocked = Clock::duration::zero();
    const auto end = Clock::now() + 400ms;
    while (Clock::now() < end) {
        const auto before = Clock::now();
        EXPECT_EQ(client.send(smallImage(), "x"), SendResult::ViewerUnavailable);
        blocked += Clock::now() - before;
        std::this_thread::sleep_for(10ms);
    }

    EXPECT_LT(blocked, 150ms);
}

TEST(ClientServer, ReportsTimeoutWhenPeerNeverReplies) {
    zmq::context_t context;
    zmq::socket_t silentPeer(context, zmq::socket_type::rep);
    silentPeer.set(zmq::sockopt::linger, 0);
    silentPeer.bind(kAnyLoopbackPort);
    Client client({.endpoint = silentPeer.get(zmq::sockopt::last_endpoint), .replyTimeout = 100ms});

    EXPECT_EQ(client.send(smallImage(), "x"), SendResult::Timeout);
}

TEST(ClientServer, ReportsRejectedWhenViewerBusy) {
    BusySink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});

    EXPECT_EQ(client.send(smallImage(), "x"), SendResult::Rejected);
}

// A sink can refuse for a reason of its own; the server answers with that reason's fixed reply text.
TEST(ClientServer, RefusesWithTheSinksReason) {
    test_support::NoLiveChannelLeftSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});

    EXPECT_EQ(client.send(smallImage(), "x"), SendResult::Rejected);

    const auto encoded = std::get<visual_debug::EncodedMessage>(visual_debug::encode(smallImage(), "y"));
    const auto pixels = encoded.pixelBytes();
    const auto reply = test_support::rawRequest(
        server.boundEndpoint(),
        {encoded.header, std::string(reinterpret_cast<const char*>(pixels.data()), pixels.size())});
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->value("status", ""), "error");
    EXPECT_EQ(reply->value("reason", ""), "too many live channels");
    EXPECT_EQ(sink.rejections(),
              (std::vector<std::string>{"too many live channels", "too many live channels"}));
}

TEST(ClientServer, RecoversAfterServerStarts) {
    const auto endpoint = unusedEndpoint();
    Client client({.endpoint = endpoint, .replyTimeout = 100ms, .retryInterval = 300ms});
    ASSERT_EQ(client.send(smallImage(), "early"), SendResult::ViewerUnavailable);

    CollectingSink sink;
    Server server(sink, {.endpoint = endpoint});
    ASSERT_TRUE(started(server));
    std::this_thread::sleep_for(400ms); // past the back-off, with time for ZeroMQ to reconnect

    EXPECT_EQ(client.send(smallImage(), "late"), SendResult::Delivered);
}

TEST(ClientServer, RepliesErrorToMalformedMessages) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));

    const std::vector<std::vector<std::string>> malformed = {{"{}", ""}, {"one frame"}, {"a", "b", "c"}};
    for (const auto& frames : malformed) {
        const auto reply = test_support::rawRequest(server.boundEndpoint(), frames);
        ASSERT_TRUE(reply.has_value()) << frames.size() << " frames";
        EXPECT_EQ(reply->value("status", ""), "error");
        const auto reason = reply->value("reason", "");
        EXPECT_FALSE(reason.empty());
        EXPECT_TRUE(test_support::isAscii(reason)) << reason;
    }
    EXPECT_EQ(sink.count(), 0u);

    Client client({.endpoint = server.boundEndpoint()});
    EXPECT_EQ(client.send(smallImage(), "after"), SendResult::Delivered);
}

TEST(ClientServer, SurvivesInvalidUtf8Header) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));

    const auto reply = test_support::rawRequest(server.boundEndpoint(), {"{\"name\":\"\xff\"}", ""});

    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->value("status", ""), "error");
    EXPECT_TRUE(server.isRunning());
    Client client({.endpoint = server.boundEndpoint()});
    EXPECT_EQ(client.send(smallImage(), "after"), SendResult::Delivered);
}

TEST(ClientServer, ReturnsInvalidImageWithoutNetwork) {
    Client client({.endpoint = unusedEndpoint(), .replyTimeout = 100ms});

    const auto start = Clock::now();
    EXPECT_EQ(client.send(cv::Mat(), "empty"), SendResult::InvalidImage);
    EXPECT_LT(Clock::now() - start, 20ms);
}

TEST(ClientServer, ReturnsInvalidEndpointWithoutThrowing) {
    for (const std::string endpoint : {"", "tcp://localhost"}) {
        Client client({.endpoint = endpoint});

        EXPECT_EQ(client.send(smallImage(), "x"), SendResult::InvalidEndpoint) << '"' << endpoint << '"';
    }
}

TEST(ClientServer, RefusesNonLoopbackBindByDefault) {
    CollectingSink sink;
    // Refused before any socket is opened, so this exposes nothing.
    Server wildcard(sink, {.endpoint = "tcp://*:*"});
    const auto refusal = wildcard.start();
    ASSERT_TRUE(refusal.has_value());
    EXPECT_TRUE(refusal->reason.starts_with("refusing to listen on [tcp://*:*]: ")) << refusal->reason;
    EXPECT_FALSE(wildcard.isRunning());

    // To show allowRemote lifts the refusal without opening a port to the network (and triggering a
    // firewall prompt), bind the loopback interface by name: real loopback, but not a literal address.
#if defined(__APPLE__)
    const char* loopbackInterface = "tcp://lo0:*";
#elif defined(__linux__)
    const char* loopbackInterface = "tcp://lo:*";
#else
    const char* loopbackInterface = nullptr;
#endif
    if (loopbackInterface == nullptr) {
        GTEST_SKIP() << "no loopback interface name known for this platform";
    }
    Server refused(sink, {.endpoint = loopbackInterface});
    EXPECT_TRUE(refused.start().has_value());

    Server allowed(sink, {.endpoint = loopbackInterface, .allowRemote = true});
    EXPECT_TRUE(started(allowed));
}

TEST(ClientServer, ReportsBindFailureWhenEndpointTaken) {
    CollectingSink sink;
    Server first(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(first));

    Server second(sink, {.endpoint = first.boundEndpoint()});

    const auto error = second.start();
    ASSERT_TRUE(error.has_value());
    EXPECT_TRUE(error->reason.starts_with("could not listen on [" + first.boundEndpoint() + "]: "))
        << error->reason;
    EXPECT_FALSE(second.isRunning());
}

TEST(ClientServer, StopsPromptly) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    ASSERT_TRUE(server.isRunning());

    const auto start = Clock::now();
    server.stop();

    EXPECT_LT(Clock::now() - start, 500ms);
    EXPECT_FALSE(server.isRunning());
    server.stop(); // idempotent
}

TEST(ClientServer, DeliversOverIpv6Loopback) {
    CollectingSink sink;
    Server server(sink, {.endpoint = "tcp://[::1]:*"});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});

    EXPECT_EQ(client.send(smallImage(), "ipv6"), SendResult::Delivered);
    EXPECT_TRUE(sink.waitForCount(1, 1s));
}

TEST(ClientServer, DoesNotCopyImagesWhileViewerIsUnavailable) {
    // A host sending a region of interest every frame must not pay for copying it while no viewer runs.
    cv::Mat whole(64, 64, CV_8UC3, cv::Scalar::all(7));
    const cv::Mat region = whole(cv::Rect(1, 1, 32, 32)); // not continuous: encoding would copy it
    Client client({.endpoint = unusedEndpoint(), .replyTimeout = 50ms, .retryInterval = 10s});
    CountingAllocator allocator;
    const DefaultAllocatorScope countAllocations(&allocator);

    const auto first = client.send(region, "x");  // waits for a connection, then starts the back-off
    const auto second = client.send(region, "x"); // inside the back-off

    EXPECT_EQ(first, SendResult::ViewerUnavailable);
    EXPECT_EQ(second, SendResult::ViewerUnavailable);
    EXPECT_EQ(allocator.count(), 0);
}

// A paused viewer keeps its port, so a sender learns it is paused instead of waiting for a timeout.
TEST(ClientServer, PausedServerRepliesViewerPausedWithoutDelivering) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    server.setPaused(true);
    const auto encoded = visual_debug::encode(smallImage(), "x");
    ASSERT_TRUE(std::holds_alternative<visual_debug::EncodedMessage>(encoded));
    const auto& message = std::get<visual_debug::EncodedMessage>(encoded);
    const auto pixels = message.pixelBytes();

    const auto reply = test_support::rawRequest(
        server.boundEndpoint(),
        {message.header, std::string(reinterpret_cast<const char*>(pixels.data()), pixels.size())});

    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->value("status", ""), "error");
    EXPECT_EQ(reply->value("reason", ""), "viewer paused");
    EXPECT_EQ(sink.count(), 0u);
    EXPECT_TRUE(server.isPaused());
}

TEST(ClientServer, ClientGetsRejectedWhilePaused) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    server.setPaused(true);
    Client client({.endpoint = server.boundEndpoint()});

    EXPECT_EQ(client.send(smallImage(), "x"), SendResult::Rejected);
    EXPECT_EQ(sink.count(), 0u);
}

TEST(ClientServer, DeliversAgainAfterResume) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});
    server.setPaused(true);
    ASSERT_EQ(client.send(smallImage(), "paused"), SendResult::Rejected);

    server.setPaused(false);

    EXPECT_FALSE(server.isPaused());
    EXPECT_EQ(client.send(smallImage(), "resumed"), SendResult::Delivered);
    ASSERT_TRUE(sink.waitForCount(1, 1s));
    EXPECT_EQ(sink.messages().front().name, "resumed");
}

TEST(ClientServer, RestartsOnTheSameEndpoint) {
    const auto endpoint = unusedEndpoint();
    CollectingSink sink;
    Server server(sink, {.endpoint = endpoint});
    ASSERT_TRUE(started(server));
    server.stop();

    ASSERT_TRUE(started(server));

    EXPECT_TRUE(server.isRunning());
    Client client({.endpoint = endpoint});
    EXPECT_EQ(client.send(smallImage(), "after restart"), SendResult::Delivered);
}

// ZeroMQ closes a socket on a background thread, so stop() must wait for the port to be released: the
// viewer's Stop says so, and Start may follow at once. A loaded machine (a CI runner) lost this race.
TEST(ClientServer, StopReleasesThePortBeforeReturning) {
    const auto endpoint = unusedEndpoint();
    CollectingSink sink;
    Server server(sink, {.endpoint = endpoint});
    for (int cycle = 0; cycle < 10; ++cycle) {
        ASSERT_TRUE(started(server)) << "cycle " << cycle;
        server.stop();
    }
}

TEST(ClientServer, StampsReceivedTime) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});
    const auto before = std::chrono::system_clock::now();

    ASSERT_EQ(client.send(smallImage(), "timed"), SendResult::Delivered);

    ASSERT_TRUE(sink.waitForCount(1, 1s));
    const auto receivedAt = sink.messages().front().receivedAt;
    EXPECT_LE(before, receivedAt);
    EXPECT_LE(receivedAt, std::chrono::system_clock::now());
}

// The viewer logs rejected requests, so every error reply must reach the sink, paused ones included.
TEST(ClientServer, ReportsEveryErrorReplyToTheSink) {
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    ASSERT_TRUE(test_support::rawRequest(server.boundEndpoint(), {"one frame"}).has_value());
    server.setPaused(true);
    Client client({.endpoint = server.boundEndpoint()});

    ASSERT_EQ(client.send(smallImage(), "x"), SendResult::Rejected);

    EXPECT_EQ(sink.rejections(), (std::vector<std::string>{"expected exactly two frames", "viewer paused"}));
}

TEST(ClientServer, ReportsBusyToTheSink) {
    BusySink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});

    ASSERT_EQ(client.send(smallImage(), "x"), SendResult::Rejected);

    EXPECT_EQ(sink.rejections(), std::vector<std::string>{"viewer busy"});
}

TEST(ClientServer, DeliversLargeImageWithinDefaultTimeout) {
    // 48 MiB took 140-360 ms on loopback during the design review; the reply deadline must scale with it.
    cv::Mat image(4096, 4096, CV_8UC3);
    cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(255));
    CollectingSink sink;
    Server server(sink, {.endpoint = kAnyLoopbackPort});
    ASSERT_TRUE(started(server));
    Client client({.endpoint = server.boundEndpoint()});

    EXPECT_EQ(client.send(image, "large"), SendResult::Delivered);

    ASSERT_TRUE(sink.waitForCount(1, 1s));
    EXPECT_EQ(cv::norm(sink.messages().front().image, image, cv::NORM_INF), 0.0);
}

} // namespace
