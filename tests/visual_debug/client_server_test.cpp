#include "test_support.h"

#include "visual_debug/client.h"
#include "visual_debug/server.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <zmq.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
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
    cv::UMatData* allocate(int dims, const int* sizes, int type, void* data, size_t* step, cv::AccessFlag flags,
                           cv::UMatUsageFlags usageFlags) const override {
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
    EXPECT_TRUE(wildcard.start().has_value());
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

    EXPECT_TRUE(second.start().has_value());
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
