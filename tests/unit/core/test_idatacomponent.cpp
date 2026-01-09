/**
 * @file test_idatacomponent.cpp
 * @brief Unit tests for IDataComponent interface
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <delila/core/IDataComponent.hpp>
#include <vector>
#include <string>

namespace DELILA {
namespace test {

/**
 * @brief Mock implementation of IDataComponent for testing
 */
class MockDataComponent : public IDataComponent {
public:
    // IComponent methods
    MOCK_METHOD(bool, Initialize, (const std::string& config_path), (override));
    MOCK_METHOD(void, Run, (), (override));
    MOCK_METHOD(void, Shutdown, (), (override));
    MOCK_METHOD(ComponentState, GetState, (), (const, override));
    MOCK_METHOD(std::string, GetComponentId, (), (const, override));
    MOCK_METHOD(ComponentStatus, GetStatus, (), (const, override));

    // IDataComponent methods
    MOCK_METHOD(void, SetInputAddresses, (const std::vector<std::string>& addresses), (override));
    MOCK_METHOD(void, SetOutputAddresses, (const std::vector<std::string>& addresses), (override));
    MOCK_METHOD(std::vector<std::string>, GetInputAddresses, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, GetOutputAddresses, (), (const, override));

    // Command channel methods
    MOCK_METHOD(void, SetCommandAddress, (const std::string& address), (override));
    MOCK_METHOD(std::string, GetCommandAddress, (), (const, override));
    MOCK_METHOD(void, StartCommandListener, (), (override));
    MOCK_METHOD(void, StopCommandListener, (), (override));

protected:
    MOCK_METHOD(bool, OnConfigure, (const nlohmann::json& config), (override));
    MOCK_METHOD(bool, OnArm, (), (override));
    MOCK_METHOD(bool, OnStart, (uint32_t run_number), (override));
    MOCK_METHOD(bool, OnStop, (bool graceful), (override));
    MOCK_METHOD(void, OnReset, (), (override));
};

class IDataComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ = std::make_unique<MockDataComponent>();
    }

    std::unique_ptr<MockDataComponent> component_;
};

TEST_F(IDataComponentTest, InheritsFromIComponent) {
    IComponent* base = component_.get();
    EXPECT_NE(base, nullptr);
}

TEST_F(IDataComponentTest, CanSetAndGetInputAddresses) {
    std::vector<std::string> addresses = {"tcp://localhost:5555", "tcp://localhost:5556"};

    EXPECT_CALL(*component_, SetInputAddresses(addresses)).Times(1);
    EXPECT_CALL(*component_, GetInputAddresses())
        .WillOnce(::testing::Return(addresses));

    component_->SetInputAddresses(addresses);
    auto result = component_->GetInputAddresses();

    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "tcp://localhost:5555");
}

TEST_F(IDataComponentTest, CanSetAndGetOutputAddresses) {
    std::vector<std::string> addresses = {"tcp://localhost:6666"};

    EXPECT_CALL(*component_, SetOutputAddresses(addresses)).Times(1);
    EXPECT_CALL(*component_, GetOutputAddresses())
        .WillOnce(::testing::Return(addresses));

    component_->SetOutputAddresses(addresses);
    auto result = component_->GetOutputAddresses();

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "tcp://localhost:6666");
}

TEST_F(IDataComponentTest, SourceComponentPattern) {
    // Source: 0 inputs, 1 output
    std::vector<std::string> no_inputs;
    std::vector<std::string> one_output = {"tcp://localhost:5555"};

    EXPECT_CALL(*component_, GetInputAddresses())
        .WillOnce(::testing::Return(no_inputs));
    EXPECT_CALL(*component_, GetOutputAddresses())
        .WillOnce(::testing::Return(one_output));

    EXPECT_TRUE(component_->GetInputAddresses().empty());
    EXPECT_EQ(component_->GetOutputAddresses().size(), 1);
}

TEST_F(IDataComponentTest, SinkComponentPattern) {
    // Sink: 1 input, 0 outputs
    std::vector<std::string> one_input = {"tcp://localhost:5555"};
    std::vector<std::string> no_outputs;

    EXPECT_CALL(*component_, GetInputAddresses())
        .WillOnce(::testing::Return(one_input));
    EXPECT_CALL(*component_, GetOutputAddresses())
        .WillOnce(::testing::Return(no_outputs));

    EXPECT_EQ(component_->GetInputAddresses().size(), 1);
    EXPECT_TRUE(component_->GetOutputAddresses().empty());
}

TEST_F(IDataComponentTest, MergerComponentPattern) {
    // Merger: N inputs, 1 output
    std::vector<std::string> multiple_inputs = {
        "tcp://localhost:5555",
        "tcp://localhost:5556",
        "tcp://localhost:5557"
    };
    std::vector<std::string> one_output = {"tcp://localhost:6666"};

    EXPECT_CALL(*component_, GetInputAddresses())
        .WillOnce(::testing::Return(multiple_inputs));
    EXPECT_CALL(*component_, GetOutputAddresses())
        .WillOnce(::testing::Return(one_output));

    EXPECT_EQ(component_->GetInputAddresses().size(), 3);
    EXPECT_EQ(component_->GetOutputAddresses().size(), 1);
}

} // namespace test
} // namespace DELILA
