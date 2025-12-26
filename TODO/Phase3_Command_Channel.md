# Phase 3: Infrastructure層 - Command通信

**見積もり**: 4時間
**目的**: REQ/REP パターンによるコマンド通信の実装
**依存**: Phase 0, Phase 1, Phase 2 完了

---

## 設計原則

- **REQ/REP パターン**: 同期的なリクエスト/レスポンス
- **JSON シリアライズ**: 人間可読で デバッグしやすい
- **タイムアウト対応**: コマンドごとに異なるタイムアウト

---

## タスク一覧

### P3-1: ZMQCommandChannel クラス実装
**見積もり**: 2時間

```cpp
// lib/net/include/infrastructure/zmq/ZMQCommandChannel.hpp
#pragma once

#include "interfaces/ICommandChannel.hpp"
#include "domain/Command.hpp"
#include "domain/CommandResponse.hpp"
#include <memory>
#include <string>
#include <zmq.hpp>

namespace DELILA::Net {

/**
 * @brief コマンドチャネル設定
 */
struct CommandChannelConfig {
    std::string address = "tcp://localhost:5557";
    bool is_server = false;  // true: REP (Component), false: REQ (Operator)
};

/**
 * @brief ZMQ コマンドチャネル実装
 *
 * - Operator側 (REQ): SendCommand → ReceiveResponse
 * - Component側 (REP): ReceiveCommand → SendResponse
 */
class ZMQCommandChannel : public ICommandChannel {
public:
    explicit ZMQCommandChannel(const CommandChannelConfig& config);
    ~ZMQCommandChannel() override;

    bool Connect() override;
    void Disconnect() override;
    bool IsConnected() const override { return connected_; }

    // Operator側
    std::optional<CommandResponse> SendCommand(
        const Command& command,
        std::chrono::milliseconds timeout) override;

    // Component側
    std::optional<Command> ReceiveCommand(
        std::chrono::milliseconds timeout) override;
    bool SendResponse(const CommandResponse& response) override;

private:
    CommandChannelConfig config_;
    std::unique_ptr<zmq::socket_t> socket_;
    bool connected_ = false;

    // JSON シリアライズ
    std::string SerializeCommand(const Command& cmd) const;
    std::optional<Command> DeserializeCommand(const std::string& json) const;
    std::string SerializeResponse(const CommandResponse& resp) const;
    std::optional<CommandResponse> DeserializeResponse(const std::string& json) const;
};

}  // namespace DELILA::Net
```

```cpp
// lib/net/src/infrastructure/zmq/ZMQCommandChannel.cpp
#include "infrastructure/zmq/ZMQCommandChannel.hpp"
#include "infrastructure/zmq/ZMQContext.hpp"
#include <nlohmann/json.hpp>

namespace DELILA::Net {

ZMQCommandChannel::ZMQCommandChannel(const CommandChannelConfig& config)
    : config_(config) {}

ZMQCommandChannel::~ZMQCommandChannel() {
    Disconnect();
}

bool ZMQCommandChannel::Connect() {
    if (connected_) return true;

    try {
        auto& ctx = ZMQContext::Instance().Get();

        if (config_.is_server) {
            // Component側: REP ソケット
            socket_ = std::make_unique<zmq::socket_t>(ctx, ZMQ_REP);
            socket_->bind(config_.address);
        } else {
            // Operator側: REQ ソケット
            socket_ = std::make_unique<zmq::socket_t>(ctx, ZMQ_REQ);
            socket_->connect(config_.address);
        }

        connected_ = true;
        return true;
    } catch (const zmq::error_t&) {
        connected_ = false;
        socket_.reset();
        return false;
    }
}

void ZMQCommandChannel::Disconnect() {
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
    connected_ = false;
}

std::optional<CommandResponse> ZMQCommandChannel::SendCommand(
    const Command& command,
    std::chrono::milliseconds timeout) {

    if (!connected_ || config_.is_server) {
        return std::nullopt;  // REQ ソケットでのみ使用可能
    }

    try {
        // タイムアウト設定
        socket_->set(zmq::sockopt::sndtimeo, static_cast<int>(timeout.count()));
        socket_->set(zmq::sockopt::rcvtimeo, static_cast<int>(timeout.count()));

        // コマンド送信
        std::string json = SerializeCommand(command);
        zmq::message_t request(json.size());
        std::memcpy(request.data(), json.c_str(), json.size());

        auto send_result = socket_->send(request, zmq::send_flags::none);
        if (!send_result.has_value()) {
            return std::nullopt;
        }

        // レスポンス受信
        zmq::message_t reply;
        auto recv_result = socket_->recv(reply, zmq::recv_flags::none);
        if (!recv_result.has_value() || reply.size() == 0) {
            return std::nullopt;
        }

        std::string reply_json(static_cast<const char*>(reply.data()), reply.size());
        return DeserializeResponse(reply_json);

    } catch (const zmq::error_t&) {
        return std::nullopt;
    }
}

std::optional<Command> ZMQCommandChannel::ReceiveCommand(
    std::chrono::milliseconds timeout) {

    if (!connected_ || !config_.is_server) {
        return std::nullopt;  // REP ソケットでのみ使用可能
    }

    try {
        socket_->set(zmq::sockopt::rcvtimeo, static_cast<int>(timeout.count()));

        zmq::message_t request;
        auto result = socket_->recv(request, zmq::recv_flags::none);
        if (!result.has_value() || request.size() == 0) {
            return std::nullopt;
        }

        std::string json(static_cast<const char*>(request.data()), request.size());
        return DeserializeCommand(json);

    } catch (const zmq::error_t&) {
        return std::nullopt;
    }
}

bool ZMQCommandChannel::SendResponse(const CommandResponse& response) {
    if (!connected_ || !config_.is_server) {
        return false;  // REP ソケットでのみ使用可能
    }

    try {
        std::string json = SerializeResponse(response);
        zmq::message_t reply(json.size());
        std::memcpy(reply.data(), json.c_str(), json.size());

        auto result = socket_->send(reply, zmq::send_flags::none);
        return result.has_value();

    } catch (const zmq::error_t&) {
        return false;
    }
}

// JSON シリアライズ実装
std::string ZMQCommandChannel::SerializeCommand(const Command& cmd) const {
    nlohmann::json j;
    j["command_type"] = CommandTypeToString(cmd.type);
    j["target_id"] = cmd.target_id;
    j["group_id"] = cmd.group_id;
    j["request_id"] = cmd.request_id;
    j["timeout_ms"] = cmd.timeout_ms;

    // params のシリアライズ
    nlohmann::json params_json;
    for (const auto& [key, value] : cmd.params) {
        std::visit([&params_json, &key](auto&& arg) {
            params_json[key] = arg;
        }, value);
    }
    j["params"] = params_json;

    // タイムスタンプ
    auto epoch = cmd.timestamp.time_since_epoch();
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    return j.dump();
}

std::optional<Command> ZMQCommandChannel::DeserializeCommand(const std::string& json) const {
    try {
        auto j = nlohmann::json::parse(json);

        Command cmd;
        cmd.type = StringToCommandType(j.value("command_type", "STATUS"));
        cmd.target_id = j.value("target_id", "");
        cmd.group_id = j.value("group_id", "");
        cmd.request_id = j.value("request_id", 0ULL);
        cmd.timeout_ms = j.value("timeout_ms", 5000U);

        // params のデシリアライズ
        if (j.contains("params") && j["params"].is_object()) {
            for (auto& [key, value] : j["params"].items()) {
                if (value.is_boolean()) {
                    cmd.params[key] = value.get<bool>();
                } else if (value.is_number_integer()) {
                    cmd.params[key] = value.get<int64_t>();
                } else if (value.is_number_float()) {
                    cmd.params[key] = value.get<double>();
                } else if (value.is_string()) {
                    cmd.params[key] = value.get<std::string>();
                }
            }
        }

        return cmd;
    } catch (...) {
        return std::nullopt;
    }
}

std::string ZMQCommandChannel::SerializeResponse(const CommandResponse& resp) const {
    nlohmann::json j;
    j["request_id"] = resp.request_id;
    j["success"] = resp.success;
    j["current_state"] = ComponentStateToString(resp.current_state);
    j["error_code"] = static_cast<uint16_t>(resp.error_code);
    j["error_message"] = resp.error_message;

    auto epoch = resp.timestamp.time_since_epoch();
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    return j.dump();
}

std::optional<CommandResponse> ZMQCommandChannel::DeserializeResponse(const std::string& json) const {
    try {
        auto j = nlohmann::json::parse(json);

        CommandResponse resp;
        resp.request_id = j.value("request_id", 0ULL);
        resp.success = j.value("success", false);
        resp.current_state = StringToComponentState(j.value("current_state", "Loaded"));
        resp.error_code = static_cast<ErrorCode>(j.value("error_code", 0));
        resp.error_message = j.value("error_message", "");

        return resp;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace DELILA::Net
```

---

### P3-2: JsonSerializer (Command用) 実装
**見積もり**: 1時間

Phase P3-1 の内部実装に含む。
必要に応じて独立クラスに分離可能。

---

### P3-3: Command通信ユニットテスト
**見積もり**: 1時間

```cpp
// tests/unit/net/infrastructure/test_command_channel.cpp

#include <gtest/gtest.h>
#include "infrastructure/zmq/ZMQCommandChannel.hpp"
#include <thread>
#include <future>

namespace DELILA::Net {

class CommandChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // サーバー（Component側）
        CommandChannelConfig server_config;
        server_config.address = "tcp://127.0.0.1:15557";
        server_config.is_server = true;
        server_ = std::make_unique<ZMQCommandChannel>(server_config);

        // クライアント（Operator側）
        CommandChannelConfig client_config;
        client_config.address = "tcp://127.0.0.1:15557";
        client_config.is_server = false;
        client_ = std::make_unique<ZMQCommandChannel>(client_config);
    }

    std::unique_ptr<ZMQCommandChannel> server_;
    std::unique_ptr<ZMQCommandChannel> client_;
};

TEST_F(CommandChannelTest, SendReceiveCommand) {
    ASSERT_TRUE(server_->Connect());
    ASSERT_TRUE(client_->Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // サーバースレッド
    auto server_future = std::async(std::launch::async, [this]() {
        auto cmd = server_->ReceiveCommand(std::chrono::seconds(5));
        if (cmd) {
            auto resp = CommandResponse::Success(cmd->request_id, ComponentState::Armed);
            server_->SendResponse(resp);
            return true;
        }
        return false;
    });

    // クライアントからコマンド送信
    Command cmd(CommandType::ARM, "digitizer_01");
    cmd.request_id = 12345;

    auto response = client_->SendCommand(cmd, std::chrono::seconds(5));

    ASSERT_TRUE(server_future.get());
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->request_id, 12345);
    EXPECT_TRUE(response->success);
    EXPECT_EQ(response->current_state, ComponentState::Armed);
}

TEST_F(CommandChannelTest, CommandWithParams) {
    ASSERT_TRUE(server_->Connect());
    ASSERT_TRUE(client_->Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto server_future = std::async(std::launch::async, [this]() {
        auto cmd = server_->ReceiveCommand(std::chrono::seconds(5));
        if (cmd) {
            // パラメータ確認
            bool has_param = cmd->params.count("trigger_threshold") > 0;
            auto resp = CommandResponse::Success(cmd->request_id, ComponentState::Configured);
            server_->SendResponse(resp);
            return has_param;
        }
        return false;
    });

    Command cmd(CommandType::CONFIGURE, "digitizer_01");
    cmd.request_id = 99;
    cmd.params["trigger_threshold"] = 100LL;

    auto response = client_->SendCommand(cmd, std::chrono::seconds(5));

    EXPECT_TRUE(server_future.get());  // パラメータが正しく渡された
    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->success);
}

TEST_F(CommandChannelTest, Timeout) {
    ASSERT_TRUE(client_->Connect());
    // サーバーは接続しない

    Command cmd(CommandType::STATUS, "test");
    auto response = client_->SendCommand(cmd, std::chrono::milliseconds(100));

    // タイムアウトで nullopt
    EXPECT_FALSE(response.has_value());
}

TEST_F(CommandChannelTest, ErrorResponse) {
    ASSERT_TRUE(server_->Connect());
    ASSERT_TRUE(client_->Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto server_future = std::async(std::launch::async, [this]() {
        auto cmd = server_->ReceiveCommand(std::chrono::seconds(5));
        if (cmd) {
            auto resp = CommandResponse::Failure(
                cmd->request_id,
                ComponentState::Error,
                ErrorCode::STATE_INVALID_TRANSITION,
                "Cannot ARM from current state"
            );
            server_->SendResponse(resp);
            return true;
        }
        return false;
    });

    Command cmd(CommandType::ARM, "digitizer_01");
    cmd.request_id = 456;

    auto response = client_->SendCommand(cmd, std::chrono::seconds(5));

    ASSERT_TRUE(server_future.get());
    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->success);
    EXPECT_EQ(response->error_code, ErrorCode::STATE_INVALID_TRANSITION);
    EXPECT_EQ(response->error_message, "Cannot ARM from current state");
}

}  // namespace DELILA::Net
```

---

---

## テスト戦略

### 既存テストの削除

このフェーズでは**削除する既存テストはない**。
Command通信は新規機能のため、既存テストは存在しない。

### 新規テスト作成

**ファイル**: `tests/unit/net/infrastructure/test_zmq_command_channel.cpp`

（上記 P3-3 セクションのテストコードを使用）

**追加テスト項目**:

```cpp
// tests/unit/net/infrastructure/test_zmq_command_channel.cpp
// （P3-3 のテストに追加）

TEST_F(CommandChannelTest, MultipleCommands) {
    ASSERT_TRUE(server_->Connect());
    ASSERT_TRUE(client_->Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 複数コマンドを順次処理
    auto server_future = std::async(std::launch::async, [this]() {
        for (int i = 0; i < 3; ++i) {
            auto cmd = server_->ReceiveCommand(std::chrono::seconds(5));
            if (!cmd) return false;

            auto resp = CommandResponse::Success(cmd->request_id, ComponentState::Running);
            if (!server_->SendResponse(resp)) return false;
        }
        return true;
    });

    for (int i = 0; i < 3; ++i) {
        Command cmd(CommandType::STATUS, "test");
        cmd.request_id = 100 + i;

        auto response = client_->SendCommand(cmd, std::chrono::seconds(5));
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response->request_id, 100 + i);
    }

    EXPECT_TRUE(server_future.get());
}

TEST_F(CommandChannelTest, ServerNotConnected) {
    ASSERT_TRUE(client_->Connect());
    // サーバー未接続

    Command cmd(CommandType::STATUS, "test");
    auto response = client_->SendCommand(cmd, std::chrono::milliseconds(100));
    EXPECT_FALSE(response.has_value());  // タイムアウト
}
```

### テストディレクトリ構造

```
tests/unit/net/infrastructure/
├── test_crc32_checksum.cpp        # Phase 2
├── test_binary_serializer.cpp     # Phase 2
├── test_zmq_data_channel.cpp      # Phase 2
├── test_zmq_status_channel.cpp    # Phase 2
└── test_zmq_command_channel.cpp   # 新規作成（このPhase）
```

---

## 完了条件

- [ ] ZMQCommandChannel が ICommandChannel を実装
- [ ] Operator側 (REQ) でコマンド送信・レスポンス受信が動作
- [ ] Component側 (REP) でコマンド受信・レスポンス送信が動作
- [ ] タイムアウトが正しく動作
- [ ] 新規 Command Channel テスト作成
- [ ] 全ユニットテストがパス

---

## JSON フォーマット

### Command JSON
```json
{
    "command_type": "CONFIGURE",
    "target_id": "digitizer_01",
    "group_id": "sources",
    "request_id": 12345,
    "timeout_ms": 30000,
    "params": {
        "trigger_threshold": 100,
        "sampling_rate": 1000000
    },
    "timestamp": 1705312200123
}
```

### CommandResponse JSON
```json
{
    "request_id": 12345,
    "success": true,
    "current_state": "Configured",
    "error_code": 0,
    "error_message": "",
    "timestamp": 1705312200456
}
```

---

## 次のPhase

Phase 3 完了後、Phase 4（Application層）に進む。
