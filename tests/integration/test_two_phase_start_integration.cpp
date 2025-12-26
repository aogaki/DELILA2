/**
 * @file test_two_phase_start_integration.cpp
 * @brief Integration tests for Two-Phase Start (ArmAcquisition/StartAcquisition)
 *
 * Tests the integration between TwoPhaseStartManager and Digitizer API.
 * Note: These tests use mock/simulated digitizers since real hardware
 * may not be available during CI/CD.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "ComponentState.hpp"
#include "TwoPhaseStartManager.hpp"

using namespace DELILA::Net;

/**
 * @brief Mock digitizer for testing two-phase start
 *
 * Simulates digitizer behavior without actual hardware
 */
class MockDigitizer {
public:
    MockDigitizer(const std::string& id) : id_(id) {}

    bool Configure()
    {
        if (state_ != State::Loaded) return false;
        state_ = State::Configured;
        return true;
    }

    bool ArmAcquisition()
    {
        if (state_ != State::Configured) return false;

        // Simulate arm delay (hardware preparation)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        state_ = State::Armed;
        return true;
    }

    bool StartAcquisition()
    {
        // If not armed, arm first (backward compatibility)
        if (state_ == State::Configured) {
            if (!ArmAcquisition()) return false;
        }

        if (state_ != State::Armed) return false;

        state_ = State::Running;
        return true;
    }

    bool StopAcquisition()
    {
        if (state_ != State::Running && state_ != State::Armed) return false;
        state_ = State::Loaded;
        return true;
    }

    bool IsArmed() const { return state_ == State::Armed; }
    bool IsRunning() const { return state_ == State::Running; }
    const std::string& GetId() const { return id_; }

private:
    enum class State { Loaded, Configured, Armed, Running };
    State state_ = State::Loaded;
    std::string id_;
};

class TwoPhaseStartIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create multiple mock digitizers
        digitizers_.push_back(std::make_unique<MockDigitizer>("dig1"));
        digitizers_.push_back(std::make_unique<MockDigitizer>("dig2"));
        digitizers_.push_back(std::make_unique<MockDigitizer>("dig3"));

        // Configure all digitizers
        for (auto& dig : digitizers_) {
            ASSERT_TRUE(dig->Configure());
        }
    }

    std::vector<std::unique_ptr<MockDigitizer>> digitizers_;
    TwoPhaseStartManager manager_;
};

// Test: Traditional single-call start (backward compatibility)
TEST_F(TwoPhaseStartIntegrationTest, TraditionalStartWorksWithoutArm)
{
    // StartAcquisition should work without explicit Arm call
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->StartAcquisition());
        EXPECT_TRUE(dig->IsRunning());
    }
}

// Test: Two-phase start with explicit Arm then Start
TEST_F(TwoPhaseStartIntegrationTest, TwoPhaseStartArmThenStart)
{
    // Phase 1: Arm all digitizers
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->ArmAcquisition());
    }

    // Verify all are armed
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->IsArmed());
        EXPECT_FALSE(dig->IsRunning());
    }

    // Phase 2: Start all digitizers
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->StartAcquisition());
    }

    // Verify all are running
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->IsRunning());
    }
}

// Test: All digitizers reach Armed state before any Start
TEST_F(TwoPhaseStartIntegrationTest, AllArmedBeforeAnyStart)
{
    // Phase 1: Arm all
    for (auto& dig : digitizers_) {
        ASSERT_TRUE(dig->ArmAcquisition());
    }

    // Verify synchronization point: all armed, none running
    size_t armed_count = 0;
    size_t running_count = 0;
    for (auto& dig : digitizers_) {
        if (dig->IsArmed()) armed_count++;
        if (dig->IsRunning()) running_count++;
    }

    EXPECT_EQ(armed_count, digitizers_.size());
    EXPECT_EQ(running_count, 0);

    // Phase 2: Start all
    for (auto& dig : digitizers_) {
        ASSERT_TRUE(dig->StartAcquisition());
    }
}

// Test: Stop resets to allow new Arm/Start cycle
TEST_F(TwoPhaseStartIntegrationTest, StopAllowsNewCycle)
{
    // First cycle
    for (auto& dig : digitizers_) {
        dig->ArmAcquisition();
        dig->StartAcquisition();
    }

    // Stop all
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->StopAcquisition());
    }

    // Re-configure for second cycle
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->Configure());
    }

    // Second cycle
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->ArmAcquisition());
    }

    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->StartAcquisition());
        EXPECT_TRUE(dig->IsRunning());
    }
}

// Test: TwoPhaseStartManager state tracking
TEST_F(TwoPhaseStartIntegrationTest, ManagerTracksState)
{
    EXPECT_EQ(manager_.GetState(), ComponentState::Loaded);

    // Configure
    EXPECT_EQ(manager_.Configure(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_.GetState(), ComponentState::Configured);

    // Arm
    EXPECT_EQ(manager_.Arm(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_.GetState(), ComponentState::Armed);
    EXPECT_TRUE(manager_.IsArmed());

    // Trigger
    EXPECT_EQ(manager_.Trigger(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_.GetState(), ComponentState::Running);
    EXPECT_TRUE(manager_.IsRunning());

    // Stop
    EXPECT_EQ(manager_.Stop(), TwoPhaseStartManager::Result::Success);
    EXPECT_EQ(manager_.GetState(), ComponentState::Loaded);
}

// Test: Manager prevents invalid state transitions
TEST_F(TwoPhaseStartIntegrationTest, ManagerPreventsInvalidTransitions)
{
    // Can't Arm before Configure
    EXPECT_EQ(manager_.Arm(), TwoPhaseStartManager::Result::InvalidState);

    manager_.Configure();

    // Can't Trigger before Arm
    EXPECT_EQ(manager_.Trigger(), TwoPhaseStartManager::Result::NotArmed);

    manager_.Arm();

    // Can't Arm twice
    EXPECT_EQ(manager_.Arm(), TwoPhaseStartManager::Result::AlreadyArmed);
}

// Test: Coordinated multi-digitizer workflow with manager
TEST_F(TwoPhaseStartIntegrationTest, CoordinatedWorkflowWithManager)
{
    // Manager tracks coordination state
    manager_.Configure();

    // Phase 1: Arm all digitizers
    bool all_armed = true;
    for (auto& dig : digitizers_) {
        if (!dig->ArmAcquisition()) {
            all_armed = false;
            break;
        }
    }
    ASSERT_TRUE(all_armed);

    // Update manager state
    EXPECT_EQ(manager_.Arm(), TwoPhaseStartManager::Result::Success);

    // Phase 2: Trigger (start all)
    EXPECT_EQ(manager_.Trigger(), TwoPhaseStartManager::Result::Success);

    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->StartAcquisition());
    }

    // All should be running
    for (auto& dig : digitizers_) {
        EXPECT_TRUE(dig->IsRunning());
    }
}

// Test: Timing - Arm phase can take time, Start should be fast
TEST_F(TwoPhaseStartIntegrationTest, ArmTakesTimeStartIsFast)
{
    // Measure Arm time
    auto arm_start = std::chrono::steady_clock::now();
    for (auto& dig : digitizers_) {
        dig->ArmAcquisition();
    }
    auto arm_end = std::chrono::steady_clock::now();
    auto arm_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(arm_end - arm_start);

    // Measure Start time
    auto start_start = std::chrono::steady_clock::now();
    for (auto& dig : digitizers_) {
        dig->StartAcquisition();
    }
    auto start_end = std::chrono::steady_clock::now();
    auto start_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(start_end - start_start);

    // Arm includes hardware prep delay (simulated 10ms per digitizer)
    // Start should be nearly instant since already armed
    EXPECT_GE(arm_duration.count(), 30);  // At least 3 x 10ms
    EXPECT_LT(start_duration.count(), 10);  // Should be very fast

    std::cout << "Arm duration: " << arm_duration.count() << "ms" << std::endl;
    std::cout << "Start duration: " << start_duration.count() << "ms" << std::endl;
}
