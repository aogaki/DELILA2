# Phase 6: 統合とテスト

**見積もり**: 4時間
**目的**: 全体の統合テストとドキュメント更新
**依存**: Phase 0-5 完了

---

## タスク一覧

### P6-1: 統合テスト作成
**見積もり**: 2時間

#### 6.1.1 Command通信統合テスト

```cpp
// tests/integration/test_command_integration.cpp

#include <gtest/gtest.h>
#include "facade/ZMQTransport.hpp"
#include "application/StateManager.hpp"
#include "application/CommandHandler.hpp"
#include <thread>
#include <future>

namespace DELILA::Net {

class CommandIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Component側
        TransportConfig component_config;
        component_config.command_address = "tcp://127.0.0.1:15560";
        component_config.bind_command = true;  // REP
        component_transport_ = std::make_unique<ZMQTransport>();
        component_transport_->Configure(component_config);

        // Operator側
        TransportConfig operator_config;
        operator_config.command_address = "tcp://127.0.0.1:15560";
        operator_config.bind_command = false;  // REQ
        operator_transport_ = std::make_unique<ZMQTransport>();
        operator_transport_->Configure(operator_config);
    }

    std::unique_ptr<ZMQTransport> component_transport_;
    std::unique_ptr<ZMQTransport> operator_transport_;
};

TEST_F(CommandIntegrationTest, FullAcquisitionCycle) {
    ASSERT_TRUE(component_transport_->Connect());
    ASSERT_TRUE(operator_transport_->Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    StateManager state_manager;

    // Component側のハンドラスレッド
    auto component_future = std::async(std::launch::async, [&]() {
        for (int i = 0; i < 4; ++i) {
            auto cmd = component_transport_->ReceiveCommand(std::chrono::seconds(5));
            if (!cmd) return false;

            auto result = state_manager.TryTransition(cmd->type);
            CommandResponse resp;
            resp.request_id = cmd->request_id;

            if (std::holds_alternative<ComponentState>(result)) {
                resp = CommandResponse::Success(cmd->request_id,
                    std::get<ComponentState>(result));
            } else {
                auto& err = std::get<TransitionError>(result);
                resp = CommandResponse::Failure(cmd->request_id,
                    state_manager.GetState(), err.code, err.message);
            }

            if (!component_transport_->SendCommandResponse(resp)) {
                return false;
            }
        }
        return true;
    });

    // Operator側: CONFIGURE → ARM → START → STOP
    Command cmd_configure(CommandType::CONFIGURE, "test_component");
    cmd_configure.request_id = 1;
    auto resp1 = operator_transport_->SendCommand(cmd_configure, std::chrono::seconds(5));
    ASSERT_TRUE(resp1.has_value());
    EXPECT_TRUE(resp1->success);
    EXPECT_EQ(resp1->current_state, ComponentState::Configured);

    Command cmd_arm(CommandType::ARM, "test_component");
    cmd_arm.request_id = 2;
    auto resp2 = operator_transport_->SendCommand(cmd_arm, std::chrono::seconds(5));
    ASSERT_TRUE(resp2.has_value());
    EXPECT_TRUE(resp2->success);
    EXPECT_EQ(resp2->current_state, ComponentState::Armed);

    Command cmd_start(CommandType::START, "test_component");
    cmd_start.request_id = 3;
    auto resp3 = operator_transport_->SendCommand(cmd_start, std::chrono::seconds(5));
    ASSERT_TRUE(resp3.has_value());
    EXPECT_TRUE(resp3->success);
    EXPECT_EQ(resp3->current_state, ComponentState::Running);

    Command cmd_stop(CommandType::STOP, "test_component");
    cmd_stop.request_id = 4;
    auto resp4 = operator_transport_->SendCommand(cmd_stop, std::chrono::seconds(5));
    ASSERT_TRUE(resp4.has_value());
    EXPECT_TRUE(resp4->success);
    EXPECT_EQ(resp4->current_state, ComponentState::Loaded);

    EXPECT_TRUE(component_future.get());
}

TEST_F(CommandIntegrationTest, ErrorRecovery) {
    ASSERT_TRUE(component_transport_->Connect());
    ASSERT_TRUE(operator_transport_->Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    StateManager state_manager;
    state_manager.TransitionToError(ErrorCode::HARDWARE_COMM_ERROR, "Test error");

    auto component_future = std::async(std::launch::async, [&]() {
        auto cmd = component_transport_->ReceiveCommand(std::chrono::seconds(5));
        if (!cmd) return false;

        auto result = state_manager.TryTransition(cmd->type);
        CommandResponse resp;

        if (std::holds_alternative<ComponentState>(result)) {
            resp = CommandResponse::Success(cmd->request_id,
                std::get<ComponentState>(result));
        } else {
            auto& err = std::get<TransitionError>(result);
            resp = CommandResponse::Failure(cmd->request_id,
                state_manager.GetState(), err.code, err.message);
        }

        return component_transport_->SendCommandResponse(resp);
    });

    // CONFIGURE でリカバリ
    Command cmd_configure(CommandType::CONFIGURE, "test_component");
    cmd_configure.request_id = 100;
    auto resp = operator_transport_->SendCommand(cmd_configure, std::chrono::seconds(5));

    EXPECT_TRUE(component_future.get());
    ASSERT_TRUE(resp.has_value());
    EXPECT_TRUE(resp->success);
    EXPECT_EQ(resp->current_state, ComponentState::Configured);
}

}  // namespace DELILA::Net
```

#### 6.1.2 データパイプライン統合テスト

```cpp
// tests/integration/test_data_pipeline_integration.cpp

TEST(DataPipelineIntegrationTest, FullRoundTrip) {
    // パイプライン構築（圧縮なし）
    auto serializer = std::make_shared<EventDataSerializer>();
    auto checksum = std::make_shared<CRC32Checksum>();

    DataPipeline<EventData> pipeline(serializer, checksum);

    // テストデータ作成
    std::vector<std::unique_ptr<EventData>> events;
    for (int i = 0; i < 100; ++i) {
        auto event = std::make_unique<EventData>();
        event->timeStampNs = i * 1000000;
        event->energy = i * 10;
        event->module = 1;
        event->channel = i % 16;
        events.push_back(std::move(event));
    }

    // 処理
    auto processed = pipeline.Process(events, 42);
    ASSERT_TRUE(processed.has_value());

    // デコード
    auto decoded = pipeline.Decode(*processed);
    ASSERT_TRUE(decoded.has_value());

    auto& [decoded_events, seq] = *decoded;
    EXPECT_EQ(seq, 42);
    EXPECT_EQ(decoded_events.size(), 100);

    // データ検証
    for (size_t i = 0; i < decoded_events.size(); ++i) {
        EXPECT_EQ(decoded_events[i]->timeStampNs, i * 1000000);
        EXPECT_EQ(decoded_events[i]->energy, i * 10);
    }
}
```

---

### P6-2: サンプルコード更新
**見積もり**: 1時間

#### 6.2.1 新API使用サンプル

```cpp
// lib/net/examples/command_example.cpp

#include "facade/ZMQTransport.hpp"
#include "application/StateManager.hpp"
#include "application/CommandHandler.hpp"
#include <iostream>
#include <thread>

using namespace DELILA::Net;

void RunComponent() {
    TransportConfig config;
    config.command_address = "tcp://*:5557";
    config.bind_command = true;  // REP ソケット

    ZMQTransport transport;
    transport.Configure(config);

    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return;
    }

    StateManager state_manager;

    std::cout << "Component waiting for commands..." << std::endl;

    while (true) {
        auto cmd = transport.ReceiveCommand(std::chrono::seconds(1));
        if (!cmd) continue;

        std::cout << "Received: " << CommandTypeToString(cmd->type)
                  << " (request_id=" << cmd->request_id << ")" << std::endl;

        auto result = state_manager.TryTransition(cmd->type);

        CommandResponse resp;
        if (std::holds_alternative<ComponentState>(result)) {
            resp = CommandResponse::Success(
                cmd->request_id,
                std::get<ComponentState>(result));
            std::cout << "  -> Success: " << ComponentStateToString(resp.current_state)
                      << std::endl;
        } else {
            auto& err = std::get<TransitionError>(result);
            resp = CommandResponse::Failure(
                cmd->request_id,
                state_manager.GetState(),
                err.code,
                err.message);
            std::cout << "  -> Error: " << err.message << std::endl;
        }

        transport.SendCommandResponse(resp);

        if (cmd->type == CommandType::STOP) {
            break;
        }
    }
}

void RunOperator() {
    TransportConfig config;
    config.command_address = "tcp://localhost:5557";
    config.bind_command = false;  // REQ ソケット

    ZMQTransport transport;
    transport.Configure(config);

    if (!transport.Connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // CONFIGURE
    Command cmd1(CommandType::CONFIGURE, "digitizer_01");
    cmd1.request_id = 1;
    auto resp1 = transport.SendCommand(cmd1);
    std::cout << "CONFIGURE: " << (resp1 && resp1->success ? "OK" : "FAIL") << std::endl;

    // ARM
    Command cmd2(CommandType::ARM, "digitizer_01");
    cmd2.request_id = 2;
    auto resp2 = transport.SendCommand(cmd2);
    std::cout << "ARM: " << (resp2 && resp2->success ? "OK" : "FAIL") << std::endl;

    // START
    Command cmd3(CommandType::START, "digitizer_01");
    cmd3.request_id = 3;
    auto resp3 = transport.SendCommand(cmd3);
    std::cout << "START: " << (resp3 && resp3->success ? "OK" : "FAIL") << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // STOP
    Command cmd4(CommandType::STOP, "digitizer_01");
    cmd4.request_id = 4;
    auto resp4 = transport.SendCommand(cmd4);
    std::cout << "STOP: " << (resp4 && resp4->success ? "OK" : "FAIL") << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <component|operator>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "component") {
        RunComponent();
    } else if (mode == "operator") {
        RunOperator();
    } else {
        std::cerr << "Unknown mode: " << mode << std::endl;
        return 1;
    }

    return 0;
}
```

---

### P6-3: ドキュメント更新
**見積もり**: 1時間

#### 6.3.1 README.md 更新

```markdown
# DELILA2 Network Library

## アーキテクチャ

lib/net は Clean Architecture に基づいて設計されています：

```
include/
├── interfaces/     # インターフェース定義
├── domain/         # ドメインオブジェクト
├── application/    # ビジネスロジック
├── infrastructure/ # 外部ライブラリ実装
├── utils/          # ユーティリティ
└── facade/         # 後方互換API
```

## 基本的な使い方

### データ送受信（既存API）

```cpp
#include "ZMQTransport.hpp"
#include "DataProcessor.hpp"

ZMQTransport transport;
TransportConfig config;
config.data_address = "tcp://*:5555";
transport.Configure(config);
transport.Connect();

DataProcessor processor;
auto processed = processor.ProcessWithAutoSequence(events);
transport.SendBytes(processed);
```

### コマンド通信（新API）

```cpp
#include "facade/ZMQTransport.hpp"
#include "domain/Command.hpp"

// Operator側
ZMQTransport transport;
transport.Configure(config);
transport.Connect();

Command cmd(CommandType::ARM, "digitizer_01");
auto response = transport.SendCommand(cmd);
if (response && response->success) {
    std::cout << "Armed!" << std::endl;
}
```

## チャネル構成

| チャネル | パターン | 用途 |
|---------|---------|------|
| Data | PUB/SUB, PUSH/PULL | イベントデータ |
| Status | PUB/SUB | コンポーネントステータス |
| Command | REQ/REP | コマンド/レスポンス |
```

#### 6.3.2 API リファレンス更新

- 新規クラスのドキュメント追加
- 既存クラスのドキュメント更新
- 使用例の追加

---

## 完了条件

- [ ] 全統合テストがパス
- [ ] サンプルコードが正しく動作
- [ ] README.md が更新されている
- [ ] API ドキュメントが最新

---

## 最終チェックリスト

### 機能確認
- [ ] データ送受信が動作
- [ ] ステータス送受信が動作
- [ ] コマンド通信が動作
- [ ] 状態遷移が正しく動作
- [ ] エラーリカバリが動作

### 品質確認
- [ ] 全ユニットテストがパス
- [ ] 全統合テストがパス
- [ ] コンパイル警告なし
- [ ] メモリリークなし（Valgrind）

### 互換性確認
- [ ] 既存テストがパス
- [ ] 既存サンプルが動作
- [ ] 既存APIが変更なしで使用可能

---

## プロジェクト完了

Phase 6 完了をもって、リファクタリングプロジェクト完了。
