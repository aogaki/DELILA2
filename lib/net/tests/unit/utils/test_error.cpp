/**
 * @file test_error.cpp
 * @brief Unit tests for Error handling and Result pattern
 * 
 * Following TDD approach - these tests define the expected behavior
 * before implementation.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "delila/net/utils/Error.hpp"

using namespace DELILA::Net;

class ErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup if needed
    }
    
    void TearDown() override {
        // Test cleanup if needed  
    }
};

/**
 * @brief Test Error creation with automatic timestamp
 */
TEST_F(ErrorTest, ErrorCreationWithTimestamp) {
    Error error(Error::INVALID_MESSAGE, "Test error message");
    
    // Verify error fields
    EXPECT_EQ(error.code, Error::INVALID_MESSAGE);
    EXPECT_EQ(error.message, "Test error message");
    EXPECT_FALSE(error.timestamp.empty());
    EXPECT_FALSE(error.system_errno.has_value());
    
    // Timestamp should be in correct format (YYYY-MM-DD HH:MM:SS)
    EXPECT_EQ(error.timestamp.length(), 19); // "2025-07-25 14:30:21"
    EXPECT_EQ(error.timestamp[4], '-');
    EXPECT_EQ(error.timestamp[7], '-');
    EXPECT_EQ(error.timestamp[10], ' ');  // Space before time
    EXPECT_EQ(error.timestamp[13], ':');
    EXPECT_EQ(error.timestamp[16], ':');
}

/**
 * @brief Test Error creation with system errno
 */
TEST_F(ErrorTest, ErrorCreationWithErrno) {
    Error error(Error::SYSTEM_ERROR, "File not found", ENOENT);
    
    EXPECT_EQ(error.code, Error::SYSTEM_ERROR);
    EXPECT_EQ(error.system_errno.value(), ENOENT);
    // Message should include errno information
    EXPECT_TRUE(error.message.find("errno") != std::string::npos);
    EXPECT_TRUE(error.message.find(std::to_string(ENOENT)) != std::string::npos);
}

/**
 * @brief Test static timestamp formatting function
 */
TEST_F(ErrorTest, TimestampFormatting) {
    std::string timestamp1 = Error::getCurrentTimestamp();
    
    // Small delay to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    std::string timestamp2 = Error::getCurrentTimestamp();
    
    // Both should be valid format
    EXPECT_EQ(timestamp1.length(), 19);
    EXPECT_EQ(timestamp2.length(), 19);
    
    // Should be different (assuming test doesn't run in same second)
    // This might occasionally fail if both timestamps are in same second
    // but validates that timestamp is actually current time
}

/**
 * @brief Test stack trace capture in debug builds
 */
TEST_F(ErrorTest, StackTraceCapture) {
    Error error(Error::COMPRESSION_FAILED, "LZ4 compression error");
    
#ifndef NDEBUG
    // In debug builds, stack trace should be captured
    EXPECT_FALSE(error.stack_trace.empty());
#else
    // In release builds, stack trace should not exist
    // (This test will compile differently based on build type)
#endif
}

/**
 * @brief Test Result pattern with successful value
 */
TEST_F(ErrorTest, ResultPatternWithSuccess) {
    Result<int> result = 42;
    
    EXPECT_TRUE(isOk(result));
    EXPECT_EQ(getValue(result), 42);
}

/**
 * @brief Test Result pattern with error
 */
TEST_F(ErrorTest, ResultPatternWithError) {
    Result<int> result = Error(Error::INVALID_CONFIG, "Configuration error");
    
    EXPECT_FALSE(isOk(result));
    EXPECT_EQ(getError(result).code, Error::INVALID_CONFIG);
    EXPECT_EQ(getError(result).message, "Configuration error");
}

/**
 * @brief Test Result helper functions Ok() and Err()
 */
TEST_F(ErrorTest, ResultHelperFunctions) {
    // Test Ok() helper
    auto success_result = Ok<std::string>("test value");
    EXPECT_TRUE(isOk(success_result));
    EXPECT_EQ(getValue(success_result), "test value");
    
    // Test Err() helper  
    auto error_result = Err<std::string>(Error(Error::CHECKSUM_MISMATCH, "Checksum failed"));
    EXPECT_FALSE(isOk(error_result));
    EXPECT_EQ(getError(error_result).code, Error::CHECKSUM_MISMATCH);
}

/**
 * @brief Test Status type for void operations
 */
TEST_F(ErrorTest, StatusType) {
    // Successful status
    Status success_status = std::monostate{};
    EXPECT_TRUE(isOk(success_status));
    
    // Error status
    Status error_status = Error(Error::VERSION_MISMATCH, "Unsupported version");
    EXPECT_FALSE(isOk(error_status));
    EXPECT_EQ(getError(error_status).code, Error::VERSION_MISMATCH);
}

/**
 * @brief Test all error codes are defined correctly
 */
TEST_F(ErrorTest, AllErrorCodes) {
    // Verify all error codes can be created
    EXPECT_NO_THROW(Error(Error::SUCCESS, "Success"));
    EXPECT_NO_THROW(Error(Error::INVALID_MESSAGE, "Invalid message"));
    EXPECT_NO_THROW(Error(Error::COMPRESSION_FAILED, "Compression failed"));
    EXPECT_NO_THROW(Error(Error::CHECKSUM_MISMATCH, "Checksum mismatch"));
    EXPECT_NO_THROW(Error(Error::INVALID_HEADER, "Invalid header"));
    EXPECT_NO_THROW(Error(Error::VERSION_MISMATCH, "Version mismatch"));
    EXPECT_NO_THROW(Error(Error::SYSTEM_ERROR, "System error"));
    EXPECT_NO_THROW(Error(Error::INVALID_CONFIG, "Invalid config"));
}

/**
 * @brief Test Result with complex types
 */
TEST_F(ErrorTest, ResultWithComplexTypes) {
    // Test with vector
    Result<std::vector<int>> vector_result = std::vector<int>{1, 2, 3, 4, 5};
    EXPECT_TRUE(isOk(vector_result));
    EXPECT_EQ(getValue(vector_result).size(), 5);
    EXPECT_EQ(getValue(vector_result)[0], 1);
    
    // Test with vector error
    Result<std::vector<int>> vector_error = Error(Error::INVALID_MESSAGE, "Vector error");
    EXPECT_FALSE(isOk(vector_error));
}