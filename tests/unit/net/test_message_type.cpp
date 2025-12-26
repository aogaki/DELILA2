/**
 * @file test_message_type.cpp
 * @brief Unit tests for MessageType enum
 */

#include <gtest/gtest.h>

#include "MessageType.hpp"

using namespace DELILA::Net;

TEST(MessageTypeTest, DataMessageHasCorrectValue)
{
    EXPECT_EQ(static_cast<uint8_t>(MessageType::Data), 0);
}

TEST(MessageTypeTest, HeartbeatMessageHasCorrectValue)
{
    EXPECT_EQ(static_cast<uint8_t>(MessageType::Heartbeat), 1);
}

TEST(MessageTypeTest, EndOfStreamMessageHasCorrectValue)
{
    EXPECT_EQ(static_cast<uint8_t>(MessageType::EndOfStream), 2);
}

TEST(MessageTypeTest, MessageTypeToString)
{
    EXPECT_EQ(MessageTypeToString(MessageType::Data), "Data");
    EXPECT_EQ(MessageTypeToString(MessageType::Heartbeat), "Heartbeat");
    EXPECT_EQ(MessageTypeToString(MessageType::EndOfStream), "EndOfStream");
}

TEST(MessageTypeTest, IsDataMessage)
{
    EXPECT_TRUE(IsDataMessage(MessageType::Data));
    EXPECT_FALSE(IsDataMessage(MessageType::Heartbeat));
    EXPECT_FALSE(IsDataMessage(MessageType::EndOfStream));
}

TEST(MessageTypeTest, IsHeartbeat)
{
    EXPECT_FALSE(IsHeartbeat(MessageType::Data));
    EXPECT_TRUE(IsHeartbeat(MessageType::Heartbeat));
    EXPECT_FALSE(IsHeartbeat(MessageType::EndOfStream));
}

TEST(MessageTypeTest, IsEndOfStream)
{
    EXPECT_FALSE(IsEndOfStream(MessageType::Data));
    EXPECT_FALSE(IsEndOfStream(MessageType::Heartbeat));
    EXPECT_TRUE(IsEndOfStream(MessageType::EndOfStream));
}
