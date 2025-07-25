/**
 * @file test_platform.cpp
 * @brief Unit tests for Platform-Specific Utilities
 * 
 * Following TDD approach - these tests define the expected behavior
 * before implementation.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "delila/net/utils/Platform.hpp"

using namespace DELILA::Net;

class PlatformTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup if needed
    }
    
    void TearDown() override {
        // Test cleanup if needed  
    }
};

/**
 * @brief Test high-resolution timestamp generation
 */
TEST_F(PlatformTest, TimestampGeneration) {
    // Get two timestamps with small delay
    uint64_t timestamp1 = getCurrentTimestampNs();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t timestamp2 = getCurrentTimestampNs();
    
    // Second timestamp should be greater than first
    EXPECT_GT(timestamp2, timestamp1);
    
    // Difference should be at least 10 milliseconds (10,000,000 nanoseconds)
    uint64_t diff = timestamp2 - timestamp1;
    EXPECT_GE(diff, 10000000ULL); // 10ms in nanoseconds
    
    // But not unreasonably large (less than 1 second)
    EXPECT_LT(diff, 1000000000ULL); // 1s in nanoseconds
}

/**
 * @brief Test Unix timestamp generation
 */
TEST_F(PlatformTest, UnixTimestampGeneration) {
    uint64_t unix_timestamp = getUnixTimestampNs();
    
    // Unix timestamp should be reasonable (after 2020, before 2030)
    // 2020-01-01 00:00:00 UTC = 1577836800 seconds = 1577836800000000000 nanoseconds
    // 2030-01-01 00:00:00 UTC = 1893456000 seconds = 1893456000000000000 nanoseconds
    EXPECT_GE(unix_timestamp, 1577836800000000000ULL); // After 2020
    EXPECT_LE(unix_timestamp, 1893456000000000000ULL); // Before 2030
}

/**
 * @brief Test timestamp precision and monotonicity
 */
TEST_F(PlatformTest, TimestampPrecision) {
    // Collect multiple timestamps rapidly
    constexpr int NUM_SAMPLES = 100;
    std::vector<uint64_t> timestamps;
    timestamps.reserve(NUM_SAMPLES);
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        timestamps.push_back(getCurrentTimestampNs());
    }
    
    // Verify monotonically increasing (or at least non-decreasing)
    for (int i = 1; i < NUM_SAMPLES; i++) {
        EXPECT_GE(timestamps[i], timestamps[i-1]);
    }
    
    // Verify nanosecond precision (some timestamps should be different)
    bool found_different = false;
    for (int i = 1; i < NUM_SAMPLES; i++) {
        if (timestamps[i] != timestamps[i-1]) {
            found_different = true;
            break;
        }
    }
    EXPECT_TRUE(found_different); // Should have nanosecond precision
}

/**
 * @brief Test endianness detection
 */
TEST_F(PlatformTest, EndiannessCheck) {
    // Platform should be little-endian
    EXPECT_TRUE(Platform::isLittleEndian());
    
    // Verify with multi-byte test
    uint32_t test_value = 0x12345678;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&test_value);
    
    // In little-endian: least significant byte first
    EXPECT_EQ(bytes[0], 0x78);  // LSB
    EXPECT_EQ(bytes[1], 0x56);
    EXPECT_EQ(bytes[2], 0x34);
    EXPECT_EQ(bytes[3], 0x12);  // MSB
}

/**
 * @brief Test platform name detection
 */
TEST_F(PlatformTest, PlatformName) {
    const char* platform_name = Platform::getPlatformName();
    
    // Should return a valid platform name
    EXPECT_NE(platform_name, nullptr);
    EXPECT_GT(strlen(platform_name), 0);
    
    // Should be one of the supported platforms
    std::string name(platform_name);
    EXPECT_TRUE(name == "Linux" || name == "macOS");
}

/**
 * @brief Test compile-time endianness assertion
 */
TEST_F(PlatformTest, CompileTimeEndianness) {
    // This test mainly verifies the code compiles
    // The actual check is done at compile-time via static_assert
    EXPECT_TRUE(true); // Placeholder
    
    // If we reach here, the compile-time assertion passed
    // (If it failed, compilation would have failed)
}

/**
 * @brief Test timestamp consistency between functions
 */
TEST_F(PlatformTest, TimestampConsistency) {
    // Get timestamps from both functions at nearly the same time
    uint64_t steady_timestamp = getCurrentTimestampNs();
    uint64_t unix_timestamp = getUnixTimestampNs();
    uint64_t steady_timestamp2 = getCurrentTimestampNs();
    
    // Unix timestamp should be much larger than steady timestamp
    // (steady_clock epoch is arbitrary, system_clock epoch is 1970)
    EXPECT_GT(unix_timestamp, steady_timestamp);
    
    // Steady timestamps should be reasonably close
    EXPECT_GE(steady_timestamp2, steady_timestamp);
    uint64_t steady_diff = steady_timestamp2 - steady_timestamp;
    EXPECT_LT(steady_diff, 1000000ULL); // Less than 1ms difference
}

/**
 * @brief Test timestamp overflow behavior
 */
TEST_F(PlatformTest, TimestampOverflow) {
    // Get current timestamp
    uint64_t current_timestamp = getCurrentTimestampNs();
    
    // Should be able to handle large values without overflow
    // (This is mainly a sanity check that we're using uint64_t)
    EXPECT_LT(current_timestamp, UINT64_MAX);
    
    // Unix timestamp should also be reasonable
    uint64_t unix_timestamp = getUnixTimestampNs();
    EXPECT_LT(unix_timestamp, UINT64_MAX);
}

/**
 * @brief Test platform utility functions thread safety
 */
TEST_F(PlatformTest, ThreadSafety) {
    constexpr int NUM_THREADS = 4;
    constexpr int CALLS_PER_THREAD = 100;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> results(NUM_THREADS);
    
    // Launch threads that call timestamp functions
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&results, t]() {
            results[t].reserve(CALLS_PER_THREAD);
            for (int i = 0; i < CALLS_PER_THREAD; i++) {
                results[t].push_back(getCurrentTimestampNs());
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all threads got reasonable results
    for (int t = 0; t < NUM_THREADS; t++) {
        EXPECT_EQ(results[t].size(), CALLS_PER_THREAD);
        
        // Verify monotonicity within each thread
        for (int i = 1; i < CALLS_PER_THREAD; i++) {
            EXPECT_GE(results[t][i], results[t][i-1]);
        }
    }
}