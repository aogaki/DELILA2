# Phase 5: Facade層 - 後方互換性

**見積もり**: 3時間
**目的**: 既存APIの維持と新アーキテクチャへの橋渡し
**依存**: Phase 0-4 完了

---

## 設計原則

- **既存API維持**: 既存のテスト・サンプルが変更なしで動作
- **内部実装の委譲**: 新しいクラスに処理を委譲
- **段階的移行**: 既存コードを徐々に新APIに移行可能

---

## タスク一覧

### P5-1: ZMQTransport ファサード作成
**見積もり**: 1.5時間

```cpp
// lib/net/include/facade/ZMQTransport.hpp
#pragma once

// 既存のヘッダーと同じパスで参照可能にする
// これにより #include "ZMQTransport.hpp" が引き続き動作

#include "domain/ComponentStatus.hpp"
#include "infrastructure/zmq/ZMQDataChannel.hpp"
#include "infrastructure/zmq/ZMQStatusChannel.hpp"
#include "infrastructure/zmq/ZMQCommandChannel.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace DELILA::Net {

/**
 * @brief Transport設定（既存APIとの互換性維持）
 */
struct TransportConfig {
    std::string data_address = "tcp://localhost:5555";
    std::string status_address = "tcp://localhost:5556";
    std::string command_address = "tcp://localhost:5557";
    bool bind_data = true;
    bool bind_status = true;
    bool bind_command = false;
    std::string data_pattern = "PUB";
    bool is_publisher = true;
};

/**
 * @brief ZMQTransport ファサード
 *
 * 既存のAPIを維持しつつ、内部では新しいチャネルクラスに委譲
 */
class ZMQTransport {
public:
    ZMQTransport();
    virtual ~ZMQTransport();

    // === 既存API（維持） ===

    bool Configure(const TransportConfig& config);
    bool ConfigureFromJSON(const nlohmann::json& config);
    bool ConfigureFromFile(const std::string& filename);
    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    // バイトベース送受信
    bool SendBytes(std::unique_ptr<std::vector<uint8_t>>& data);
    std::unique_ptr<std::vector<uint8_t>> ReceiveBytes();

    // ステータス送受信
    bool SendStatus(const ComponentStatus& status);
    std::unique_ptr<ComponentStatus> ReceiveStatus();

    // === 新API（追加） ===

    /**
     * @brief コマンドを送信（Operator側）
     */
    std::optional<CommandResponse> SendCommand(
        const Command& command,
        std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /**
     * @brief コマンドを受信（Component側）
     */
    std::optional<Command> ReceiveCommand(
        std::chrono::milliseconds timeout = std::chrono::seconds(1));

    /**
     * @brief レスポンスを送信（Component側）
     */
    bool SendCommandResponse(const CommandResponse& response);

    // === 内部チャネルへのアクセス（高度な使用向け） ===

    IDataChannel* GetDataChannel() { return data_channel_.get(); }
    IStatusChannel* GetStatusChannel() { return status_channel_.get(); }
    ICommandChannel* GetCommandChannel() { return command_channel_.get(); }

private:
    TransportConfig config_;
    bool configured_ = false;
    bool connected_ = false;

    std::unique_ptr<ZMQDataChannel> data_channel_;
    std::unique_ptr<ZMQStatusChannel> status_channel_;
    std::unique_ptr<ZMQCommandChannel> command_channel_;

    void CreateChannels();
};

}  // namespace DELILA::Net
```

```cpp
// lib/net/src/facade/ZMQTransport.cpp
#include "facade/ZMQTransport.hpp"
#include <fstream>

namespace DELILA::Net {

ZMQTransport::ZMQTransport() = default;

ZMQTransport::~ZMQTransport() {
    Disconnect();
}

bool ZMQTransport::Configure(const TransportConfig& config) {
    if (config.data_address.empty()) {
        return false;
    }

    // パターン検証
    if (config.data_pattern != "PUB" && config.data_pattern != "SUB" &&
        config.data_pattern != "PUSH" && config.data_pattern != "PULL" &&
        config.data_pattern != "PAIR") {
        return false;
    }

    config_ = config;
    configured_ = true;
    return true;
}

bool ZMQTransport::ConfigureFromJSON(const nlohmann::json& config) {
    try {
        TransportConfig transport_config;

        if (config.contains("data_address")) {
            transport_config.data_address = config["data_address"];
        }
        if (config.contains("status_address")) {
            transport_config.status_address = config["status_address"];
        }
        if (config.contains("command_address")) {
            transport_config.command_address = config["command_address"];
        }
        if (config.contains("data_pattern")) {
            transport_config.data_pattern = config["data_pattern"];
        }
        if (config.contains("bind_data")) {
            transport_config.bind_data = config["bind_data"];
        }
        if (config.contains("bind_status")) {
            transport_config.bind_status = config["bind_status"];
        }
        if (config.contains("bind_command")) {
            transport_config.bind_command = config["bind_command"];
        }
        if (config.contains("is_publisher")) {
            transport_config.is_publisher = config["is_publisher"];
        }

        return Configure(transport_config);
    } catch (...) {
        return false;
    }
}

bool ZMQTransport::ConfigureFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        nlohmann::json config = nlohmann::json::parse(file);
        return ConfigureFromJSON(config);
    } catch (...) {
        return false;
    }
}

void ZMQTransport::CreateChannels() {
    // データチャネル
    DataChannelConfig data_config;
    data_config.address = config_.data_address;
    data_config.pattern = config_.data_pattern;
    data_config.bind = config_.bind_data;
    data_channel_ = std::make_unique<ZMQDataChannel>(data_config);

    // ステータスチャネル（アドレスが異なる場合のみ）
    if (config_.status_address != config_.data_address) {
        StatusChannelConfig status_config;
        status_config.address = config_.status_address;
        status_config.bind = config_.bind_status;
        status_channel_ = std::make_unique<ZMQStatusChannel>(
            status_config, config_.is_publisher);
    }

    // コマンドチャネル（アドレスが設定されている場合）
    if (!config_.command_address.empty()) {
        CommandChannelConfig cmd_config;
        cmd_config.address = config_.command_address;
        cmd_config.is_server = config_.bind_command;
        command_channel_ = std::make_unique<ZMQCommandChannel>(cmd_config);
    }
}

bool ZMQTransport::Connect() {
    if (!configured_) return false;
    if (connected_) return true;

    try {
        CreateChannels();

        if (data_channel_ && !data_channel_->Connect()) {
            return false;
        }
        if (status_channel_ && !status_channel_->Connect()) {
            return false;
        }
        if (command_channel_ && !command_channel_->Connect()) {
            return false;
        }

        connected_ = true;
        return true;
    } catch (...) {
        Disconnect();
        return false;
    }
}

void ZMQTransport::Disconnect() {
    if (data_channel_) data_channel_->Disconnect();
    if (status_channel_) status_channel_->Disconnect();
    if (command_channel_) command_channel_->Disconnect();

    data_channel_.reset();
    status_channel_.reset();
    command_channel_.reset();
    connected_ = false;
}

bool ZMQTransport::IsConnected() const {
    return connected_;
}

bool ZMQTransport::SendBytes(std::unique_ptr<std::vector<uint8_t>>& data) {
    if (!connected_ || !data_channel_ || !data) return false;

    bool result = data_channel_->Send(*data);
    if (result) {
        data.reset();  // 既存の動作を維持
    }
    return result;
}

std::unique_ptr<std::vector<uint8_t>> ZMQTransport::ReceiveBytes() {
    if (!connected_ || !data_channel_) return nullptr;

    auto result = data_channel_->Receive();
    if (result) {
        return std::make_unique<std::vector<uint8_t>>(std::move(*result));
    }
    return nullptr;
}

bool ZMQTransport::SendStatus(const ComponentStatus& status) {
    if (!connected_ || !status_channel_) return false;
    return status_channel_->SendStatus(status);
}

std::unique_ptr<ComponentStatus> ZMQTransport::ReceiveStatus() {
    if (!connected_ || !status_channel_) return nullptr;

    auto result = status_channel_->ReceiveStatus();
    if (result) {
        return std::make_unique<ComponentStatus>(std::move(*result));
    }
    return nullptr;
}

std::optional<CommandResponse> ZMQTransport::SendCommand(
    const Command& command,
    std::chrono::milliseconds timeout) {
    if (!connected_ || !command_channel_) return std::nullopt;
    return command_channel_->SendCommand(command, timeout);
}

std::optional<Command> ZMQTransport::ReceiveCommand(
    std::chrono::milliseconds timeout) {
    if (!connected_ || !command_channel_) return std::nullopt;
    return command_channel_->ReceiveCommand(timeout);
}

bool ZMQTransport::SendCommandResponse(const CommandResponse& response) {
    if (!connected_ || !command_channel_) return false;
    return command_channel_->SendResponse(response);
}

}  // namespace DELILA::Net
```

---

### P5-2: DataProcessor ファサード作成
**見積もり**: 1時間

```cpp
// lib/net/include/facade/DataProcessor.hpp
#pragma once

#include "application/DataPipeline.hpp"
#include "infrastructure/serialization/BinarySerializer.hpp"
#include "infrastructure/checksum/CRC32Checksum.hpp"
#include <atomic>
#include <memory>

namespace DELILA::Net {

// ヘッダー定義（既存と同じ）
struct BinaryDataHeader {
    uint64_t magic_number;
    uint64_t sequence_number;
    uint32_t format_version;
    uint32_t header_size;
    uint32_t event_count;
    uint32_t data_size;
    uint32_t checksum;
    uint64_t timestamp;
    uint8_t checksum_type;
    uint8_t reserved[15];
};

constexpr uint32_t BINARY_DATA_HEADER_SIZE = 64;
constexpr uint64_t BINARY_DATA_MAGIC_NUMBER = 0x44454C494C413200;
constexpr uint32_t FORMAT_VERSION_EVENTDATA = 1;
constexpr uint32_t FORMAT_VERSION_MINIMAL_EVENTDATA = 2;
constexpr uint8_t CHECKSUM_NONE = 0;
constexpr uint8_t CHECKSUM_CRC32 = 1;

/**
 * @brief DataProcessor ファサード
 *
 * 既存のAPIを維持しつつ、内部では新しいクラスに委譲
 */
class DataProcessor {
public:
    DataProcessor();
    ~DataProcessor() = default;

    // === 既存API（維持） ===

    void EnableChecksum(bool enable = true);
    bool IsChecksumEnabled() const { return checksum_enabled_; }

    // 処理メソッド
    std::unique_ptr<std::vector<uint8_t>> Process(
        const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events,
        uint64_t sequence_number);

    std::unique_ptr<std::vector<uint8_t>> Process(
        const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>& events,
        uint64_t sequence_number);

    std::pair<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>, uint64_t>
    Decode(const std::unique_ptr<std::vector<uint8_t>>& data);

    std::pair<std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>, uint64_t>
    DecodeMinimal(const std::unique_ptr<std::vector<uint8_t>>& data);

    // シーケンス管理
    uint64_t GetNextSequence();
    uint64_t GetCurrentSequence() const;
    void ResetSequence();

    // 自動シーケンス処理
    std::unique_ptr<std::vector<uint8_t>> ProcessWithAutoSequence(
        const std::unique_ptr<std::vector<std::unique_ptr<EventData>>>& events);

    std::unique_ptr<std::vector<uint8_t>> ProcessWithAutoSequence(
        const std::unique_ptr<std::vector<std::unique_ptr<MinimalEventData>>>& events);

    // 静的CRCメソッド（テスト互換性のため）
    static uint32_t CalculateCRC32(const uint8_t* data, size_t length);
    static bool VerifyCRC32(const uint8_t* data, size_t length, uint32_t expected);

private:
    bool checksum_enabled_ = true;
    std::atomic<uint64_t> sequence_counter_{0};

    // 内部コンポーネント
    std::shared_ptr<EventDataSerializer> event_serializer_;
    std::shared_ptr<MinimalEventDataSerializer> minimal_serializer_;
    std::shared_ptr<CRC32Checksum> checksum_;
};

}  // namespace DELILA::Net
```

---

### P5-3: 既存テストの動作確認
**見積もり**: 30分

```bash
# 全テストを実行
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j
ctest --output-on-failure

# 特に以下のテストが通ることを確認
ctest -R "test_zmq_transport" -V
ctest -R "test_data_processor" -V
ctest -R "test_serializer" -V
```

**確認項目**:
- [ ] `test_zmq_transport_bytes` がパス
- [ ] `test_zmq_transport_minimal` がパス
- [ ] `test_data_processor` がパス
- [ ] `test_data_processor_sequence` がパス
- [ ] `test_serializer_format_versions` がパス
- [ ] 統合テストがパス

---

## ヘッダーファイルの互換性

既存コードが以下のincludeで動作し続けるよう、シンボリックリンクまたは転送ヘッダーを配置:

```cpp
// lib/net/include/ZMQTransport.hpp (転送ヘッダー)
#pragma once
#include "facade/ZMQTransport.hpp"

// lib/net/include/DataProcessor.hpp (転送ヘッダー)
#pragma once
#include "facade/DataProcessor.hpp"
```

---

---

## テスト戦略

### 既存テストの対応

このPhaseでは、Phase 2 で削除予定だったテストの代替として **Facade層用のテスト** を作成する。

既存テストの多くは Facade を経由して動作するため、**Facadeが完成すれば動作する**はず。
ただし、新アーキテクチャに合わせて再作成する。

### 新規テスト作成

**ファイル**: `tests/unit/net/facade/test_zmq_transport_facade.cpp`

```cpp
// tests/unit/net/facade/test_zmq_transport_facade.cpp
#include <gtest/gtest.h>
#include "facade/ZMQTransport.hpp"
#include <thread>

namespace DELILA::Net {

class ZMQTransportFacadeTest : public ::testing::Test {
protected:
    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

TEST_F(ZMQTransportFacadeTest, ConfigureWithValidConfig) {
    ZMQTransport transport;

    TransportConfig config;
    config.data_address = "tcp://127.0.0.1:25555";
    config.data_pattern = "PUSH";
    config.bind_data = true;

    EXPECT_TRUE(transport.Configure(config));
}

TEST_F(ZMQTransportFacadeTest, ConfigureWithEmptyAddress) {
    ZMQTransport transport;

    TransportConfig config;
    config.data_address = "";  // 空

    EXPECT_FALSE(transport.Configure(config));
}

TEST_F(ZMQTransportFacadeTest, ConfigureWithInvalidPattern) {
    ZMQTransport transport;

    TransportConfig config;
    config.data_address = "tcp://127.0.0.1:25555";
    config.data_pattern = "INVALID";  // 無効なパターン

    EXPECT_FALSE(transport.Configure(config));
}

TEST_F(ZMQTransportFacadeTest, ConnectWithoutConfigure) {
    ZMQTransport transport;
    EXPECT_FALSE(transport.Connect());
}

TEST_F(ZMQTransportFacadeTest, SendReceiveBytes) {
    // Pusher
    ZMQTransport pusher;
    TransportConfig push_config;
    push_config.data_address = "tcp://127.0.0.1:25556";
    push_config.data_pattern = "PUSH";
    push_config.bind_data = true;
    ASSERT_TRUE(pusher.Configure(push_config));
    ASSERT_TRUE(pusher.Connect());

    // Puller
    ZMQTransport puller;
    TransportConfig pull_config;
    pull_config.data_address = "tcp://127.0.0.1:25556";
    pull_config.data_pattern = "PULL";
    pull_config.bind_data = false;
    ASSERT_TRUE(puller.Configure(pull_config));
    ASSERT_TRUE(puller.Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 送信
    auto data = std::make_unique<std::vector<uint8_t>>(
        std::vector<uint8_t>{1, 2, 3, 4, 5});
    EXPECT_TRUE(pusher.SendBytes(data));
    EXPECT_EQ(data, nullptr);  // 所有権移動

    // 受信
    auto received = puller.ReceiveBytes();
    ASSERT_NE(received, nullptr);
    EXPECT_EQ(received->size(), 5);
    EXPECT_EQ((*received)[0], 1);
}

TEST_F(ZMQTransportFacadeTest, SendReceiveStatus) {
    // Publisher
    ZMQTransport publisher;
    TransportConfig pub_config;
    pub_config.data_address = "tcp://127.0.0.1:25557";
    pub_config.status_address = "tcp://127.0.0.1:25558";
    pub_config.data_pattern = "PUB";
    pub_config.is_publisher = true;
    ASSERT_TRUE(publisher.Configure(pub_config));
    ASSERT_TRUE(publisher.Connect());

    // Subscriber
    ZMQTransport subscriber;
    TransportConfig sub_config;
    sub_config.data_address = "tcp://127.0.0.1:25557";
    sub_config.status_address = "tcp://127.0.0.1:25558";
    sub_config.data_pattern = "SUB";
    sub_config.bind_data = false;
    sub_config.bind_status = false;
    sub_config.is_publisher = false;
    ASSERT_TRUE(subscriber.Configure(sub_config));
    ASSERT_TRUE(subscriber.Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ComponentStatus status;
    status.component_id = "test";
    status.state = "Running";
    status.heartbeat_counter = 42;

    EXPECT_TRUE(publisher.SendStatus(status));

    auto received = subscriber.ReceiveStatus();
    ASSERT_NE(received, nullptr);
    EXPECT_EQ(received->component_id, "test");
    EXPECT_EQ(received->heartbeat_counter, 42);
}

TEST_F(ZMQTransportFacadeTest, SendReceiveCommand) {
    // Component (REP)
    ZMQTransport component;
    TransportConfig comp_config;
    comp_config.data_address = "tcp://127.0.0.1:25559";
    comp_config.command_address = "tcp://127.0.0.1:25560";
    comp_config.bind_command = true;  // REP
    ASSERT_TRUE(component.Configure(comp_config));
    ASSERT_TRUE(component.Connect());

    // Operator (REQ)
    ZMQTransport oper;
    TransportConfig oper_config;
    oper_config.data_address = "tcp://127.0.0.1:25559";
    oper_config.command_address = "tcp://127.0.0.1:25560";
    oper_config.bind_data = false;
    oper_config.bind_command = false;  // REQ
    ASSERT_TRUE(oper.Configure(oper_config));
    ASSERT_TRUE(oper.Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Component側スレッド
    auto comp_future = std::async(std::launch::async, [&]() {
        auto cmd = component.ReceiveCommand(std::chrono::seconds(5));
        if (!cmd) return false;

        auto resp = CommandResponse::Success(cmd->request_id, ComponentState::Armed);
        return component.SendCommandResponse(resp);
    });

    // Operator側: コマンド送信
    Command cmd(CommandType::ARM, "test");
    cmd.request_id = 123;

    auto response = oper.SendCommand(cmd, std::chrono::seconds(5));

    EXPECT_TRUE(comp_future.get());
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->request_id, 123);
    EXPECT_TRUE(response->success);
    EXPECT_EQ(response->current_state, ComponentState::Armed);
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/facade/test_data_processor_facade.cpp`

```cpp
// tests/unit/net/facade/test_data_processor_facade.cpp
#include <gtest/gtest.h>
#include "facade/DataProcessor.hpp"

namespace DELILA::Net {

TEST(DataProcessorFacadeTest, ProcessEventData) {
    DataProcessor processor;

    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    auto event = std::make_unique<EventData>();
    event->timeStampNs = 1234567890;
    event->energy = 1000;
    events->push_back(std::move(event));

    auto processed = processor.Process(events, 42);
    ASSERT_NE(processed, nullptr);
    EXPECT_FALSE(processed->empty());

    // ヘッダー確認
    ASSERT_GE(processed->size(), BINARY_DATA_HEADER_SIZE);
    auto* header = reinterpret_cast<const BinaryDataHeader*>(processed->data());
    EXPECT_EQ(header->magic_number, BINARY_DATA_MAGIC_NUMBER);
    EXPECT_EQ(header->sequence_number, 42);
    EXPECT_EQ(header->event_count, 1);
}

TEST(DataProcessorFacadeTest, ProcessMinimalEventData) {
    DataProcessor processor;

    auto events = std::make_unique<std::vector<std::unique_ptr<MinimalEventData>>>();
    auto event = std::make_unique<MinimalEventData>();
    event->timestamp = 9876543210ULL;
    event->energy = 500;
    events->push_back(std::move(event));

    auto processed = processor.Process(events, 100);
    ASSERT_NE(processed, nullptr);
    EXPECT_FALSE(processed->empty());
}

TEST(DataProcessorFacadeTest, DecodeEventData) {
    DataProcessor processor;

    // エンコード
    auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
    auto event = std::make_unique<EventData>();
    event->timeStampNs = 1234567890;
    event->energy = 1000;
    events->push_back(std::move(event));

    auto processed = processor.Process(events, 42);
    ASSERT_NE(processed, nullptr);

    // デコード
    auto [decoded, seq] = processor.Decode(processed);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(seq, 42);
    ASSERT_EQ(decoded->size(), 1);
    EXPECT_EQ((*decoded)[0]->timeStampNs, 1234567890);
    EXPECT_EQ((*decoded)[0]->energy, 1000);
}

TEST(DataProcessorFacadeTest, ProcessWithAutoSequence) {
    DataProcessor processor;

    processor.ResetSequence();

    for (int i = 0; i < 3; ++i) {
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        events->push_back(std::make_unique<EventData>());

        auto processed = processor.ProcessWithAutoSequence(events);
        ASSERT_NE(processed, nullptr);

        auto* header = reinterpret_cast<const BinaryDataHeader*>(processed->data());
        EXPECT_EQ(header->sequence_number, i);
    }

    EXPECT_EQ(processor.GetCurrentSequence(), 3);
}

TEST(DataProcessorFacadeTest, ChecksumEnabledDisabled) {
    DataProcessor processor;

    EXPECT_TRUE(processor.IsChecksumEnabled());  // デフォルト有効

    processor.EnableChecksum(false);
    EXPECT_FALSE(processor.IsChecksumEnabled());

    processor.EnableChecksum(true);
    EXPECT_TRUE(processor.IsChecksumEnabled());
}

TEST(DataProcessorFacadeTest, StaticCRC32Methods) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    uint32_t crc = DataProcessor::CalculateCRC32(data.data(), data.size());

    EXPECT_TRUE(DataProcessor::VerifyCRC32(data.data(), data.size(), crc));
    EXPECT_FALSE(DataProcessor::VerifyCRC32(data.data(), data.size(), crc + 1));
}

}  // namespace DELILA::Net
```

### テストディレクトリ構造

```
tests/unit/net/
├── facade/                             # 新規ディレクトリ
│   ├── test_zmq_transport_facade.cpp   # 新規作成
│   └── test_data_processor_facade.cpp  # 新規作成
└── ... (他のディレクトリ)
```

### CMakeLists.txt 更新

```cmake
# tests/unit/net/facade のテストを追加
add_executable(delila_net_facade_tests
    facade/test_zmq_transport_facade.cpp
    facade/test_data_processor_facade.cpp
)

target_link_libraries(delila_net_facade_tests
    delila_net
    delila_core
    GTest::gtest_main
)

add_test(NAME NetFacadeTests COMMAND delila_net_facade_tests)
```

---

## 完了条件

- [ ] ZMQTransport ファサードが既存APIを維持
- [ ] DataProcessor ファサードが既存APIを維持
- [ ] 新規 Facade テスト作成
- [ ] 全ユニットテストがパス
- [ ] 新API（SendCommand等）が動作
- [ ] 既存サンプルコードが変更なしで動作

---

## 次のPhase

Phase 5 完了後、Phase 6（統合とテスト）に進む。
