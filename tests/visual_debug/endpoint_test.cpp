#include "test_support.h"

#include "visual_debug/endpoint.h"

#include <gtest/gtest.h>

namespace {

using visual_debug::defaultEndpoint;
using visual_debug::isLoopbackEndpoint;
using visual_debug::kEndpointEnvironmentVariable;

class Endpoint : public testing::Test {
protected:
    void SetUp() override { test_support::unsetEnvironment(kEndpointEnvironmentVariable); }
    void TearDown() override { test_support::unsetEnvironment(kEndpointEnvironmentVariable); }
};

TEST_F(Endpoint, DefaultsToLoopback) { EXPECT_EQ(defaultEndpoint(), "tcp://127.0.0.1:7139"); }

TEST_F(Endpoint, ReadsEnvironmentVariable) {
    test_support::setEnvironment(kEndpointEnvironmentVariable, "tcp://127.0.0.1:9000");

    EXPECT_EQ(defaultEndpoint(), "tcp://127.0.0.1:9000");
}

TEST_F(Endpoint, TreatsEmptyVariableAsUnset) {
    test_support::setEnvironment(kEndpointEnvironmentVariable, "");

    EXPECT_EQ(defaultEndpoint(), "tcp://127.0.0.1:7139");
}

TEST_F(Endpoint, RecognisesLoopback) {
    EXPECT_TRUE(isLoopbackEndpoint("tcp://127.0.0.1:1"));
    EXPECT_TRUE(isLoopbackEndpoint("tcp://[::1]:1"));
    EXPECT_TRUE(isLoopbackEndpoint("ipc://x"));
    EXPECT_TRUE(isLoopbackEndpoint("inproc://x"));

    EXPECT_FALSE(isLoopbackEndpoint("tcp://*:1"));
    EXPECT_FALSE(isLoopbackEndpoint("tcp://0.0.0.0:1"));
    EXPECT_FALSE(isLoopbackEndpoint("tcp://192.168.1.2:1"));
    // A hosts file can point "localhost" anywhere, so only literal loopback addresses count.
    EXPECT_FALSE(isLoopbackEndpoint("tcp://localhost:1"));
    EXPECT_FALSE(isLoopbackEndpoint("tcp://127.0.0.1.example.com:1"));
}

} // namespace
