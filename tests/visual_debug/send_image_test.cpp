#include "test_support.h"

#include "visual_debug/endpoint.h"
#include "visual_debug/send_image.h"
#include "visual_debug/server.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

// The only test that calls sendImage: its process-wide client reads the endpoint once, on first use.
// (gtest_discover_tests also runs every test in its own process.)
TEST(SendImage, IsSafeFromMultipleThreads) {
    test_support::CollectingSink sink;
    visual_debug::Server server(sink, {.endpoint = "tcp://127.0.0.1:*"});
    ASSERT_FALSE(server.start().has_value());
    test_support::setEnvironment(visual_debug::kEndpointEnvironmentVariable, server.boundEndpoint().c_str());
    const cv::Mat image(3, 4, CV_8UC1, cv::Scalar(7));

    std::atomic<int> delivered{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < 5; ++i) {
                const auto name = "thread " + std::to_string(t) + " image " + std::to_string(i);
                if (visual_debug::sendImage(image, name) == visual_debug::SendResult::Delivered) {
                    ++delivered;
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(delivered.load(), 20);
    EXPECT_TRUE(sink.waitForCount(20, 1s));
    test_support::unsetEnvironment(visual_debug::kEndpointEnvironmentVariable);
}

} // namespace
