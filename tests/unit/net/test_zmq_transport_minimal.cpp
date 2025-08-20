// This file was removed because it contained over-engineered tests for 
// SendMinimal/ReceiveMinimal methods that violate the KISS principle.
// 
// ZMQTransport should only handle raw bytes, not know about specific data types.
// The proper architecture is:
// 1. DataProcessor serializes MinimalEventData to bytes
// 2. ZMQTransport sends/receives raw bytes  
// 3. Integration tests verify the complete pipeline
//
// The minimal event data transport functionality is tested in integration tests.

#include <gtest/gtest.h>

// Placeholder test to ensure this file compiles
TEST(ZMQTransportMinimalTest, PlaceholderTest) {
    EXPECT_TRUE(true);
}