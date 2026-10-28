#include "send_counter.h"

#include <gtest/gtest.h>

namespace {

using visual_debug::SendResult;

TEST(SendCounter, StartsAtOne) { EXPECT_EQ(SendCounter().nameFor("a.png"), "a.png #1"); }

TEST(SendCounter, KeepsNumberAfterFailure) {
    SendCounter counter;
    counter.record(SendResult::ViewerUnavailable);
    counter.record(SendResult::Rejected);
    counter.record(SendResult::InvalidImage);
    counter.record(SendResult::InvalidEndpoint);
    EXPECT_EQ(counter.nameFor("a.png"), "a.png #1");
}

// A timed-out image may still have reached the viewer, so its number is not reused.
TEST(SendCounter, AdvancesAfterDeliveredAndTimeout) {
    SendCounter counter;
    counter.record(SendResult::Delivered);
    EXPECT_EQ(counter.nameFor("a.png"), "a.png #2");
    counter.record(SendResult::Timeout);
    EXPECT_EQ(counter.nameFor("a.png"), "a.png #3");
}

} // namespace
