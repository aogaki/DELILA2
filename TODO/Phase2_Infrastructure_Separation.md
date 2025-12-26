# Phase 2: Infrastructure層 - 分離

**見積もり**: 6時間
**目的**: 既存コードを責任ごとに分離、LZ4圧縮を削除
**依存**: Phase 0, Phase 1 完了

---

## 設計原則

- **単一責任**: 各クラスは1つの責任のみ
- **インターフェース実装**: Phase 0 で定義したインターフェースを実装
- **外部依存の隔離**: zmq はこの層に閉じ込める
- **LZ4削除**: 圧縮機能は不要のため完全に削除

---

## タスク一覧

### P2-1: CRC32Checksum クラス抽出
**見積もり**: 1時間

```cpp
// lib/net/include/infrastructure/checksum/CRC32Checksum.hpp
#pragma once

#include "interfaces/IChecksum.hpp"
#include <array>
#include <cstdint>

namespace DELILA::Net {

/**
 * @brief CRC32 チェックサム実装
 *
 * DataProcessor から抽出。
 */
class CRC32Checksum : public IChecksum {
public:
    CRC32Checksum();
    ~CRC32Checksum() override = default;

    uint32_t Calculate(std::span<const uint8_t> data) override;
    bool Verify(std::span<const uint8_t> data, uint32_t expected) override;
    uint8_t GetTypeId() const override { return 1; }  // CHECKSUM_CRC32

private:
    static constexpr uint32_t POLYNOMIAL = 0xEDB88320;
    std::array<uint32_t, 256> table_;

    void InitializeTable();
};

}  // namespace DELILA::Net
```

```cpp
// lib/net/src/infrastructure/checksum/CRC32Checksum.cpp
#include "infrastructure/checksum/CRC32Checksum.hpp"

namespace DELILA::Net {

CRC32Checksum::CRC32Checksum() {
    InitializeTable();
}

void CRC32Checksum::InitializeTable() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        table_[i] = crc;
    }
}

uint32_t CRC32Checksum::Calculate(std::span<const uint8_t> data) {
    uint32_t crc = 0xFFFFFFFF;
    for (auto byte : data) {
        crc = table_[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

bool CRC32Checksum::Verify(std::span<const uint8_t> data, uint32_t expected) {
    return Calculate(data) == expected;
}

}  // namespace DELILA::Net
```

**テスト項目**:
- [ ] 既知の入力に対してCRC32が正しく計算される
- [ ] Verify が正しく動作する
- [ ] 空データに対しても動作する

---

### P2-2: BinarySerializer クラス抽出
**見積もり**: 2時間

```cpp
// lib/net/include/infrastructure/serialization/BinarySerializer.hpp
#pragma once

#include "interfaces/ISerializer.hpp"
#include <delila/core/EventData.hpp>
#include <delila/core/MinimalEventData.hpp>

namespace DELILA::Net {

/**
 * @brief EventData のバイナリシリアライズ実装
 */
class EventDataSerializer : public IEventDataSerializer {
public:
    std::vector<uint8_t> Serialize(
        const std::vector<std::unique_ptr<DELILA::Digitizer::EventData>>& objects) override;

    std::optional<std::vector<std::unique_ptr<DELILA::Digitizer::EventData>>> Deserialize(
        std::span<const uint8_t> data) override;
};

/**
 * @brief MinimalEventData のバイナリシリアライズ実装
 */
class MinimalEventDataSerializer : public IMinimalEventDataSerializer {
public:
    static constexpr size_t RECORD_SIZE = 22;  // MinimalEventData サイズ

    std::vector<uint8_t> Serialize(
        const std::vector<std::unique_ptr<DELILA::Digitizer::MinimalEventData>>& objects) override;

    std::optional<std::vector<std::unique_ptr<DELILA::Digitizer::MinimalEventData>>> Deserialize(
        std::span<const uint8_t> data) override;
};

}  // namespace DELILA::Net
```

**実装**: DataProcessor.cpp の Serialize/Deserialize をそのまま移植

**テスト項目**:
- [ ] シリアライズ→デシリアライズで元データと一致
- [ ] 空データに対して動作
- [ ] 不正データに対して nullopt を返す

---

### P2-3: ZMQContext 共有クラス作成
**見積もり**: 30分

```cpp
// lib/net/include/infrastructure/zmq/ZMQContext.hpp
#pragma once

#include <memory>
#include <zmq.hpp>

namespace DELILA::Net {

/**
 * @brief ZMQ コンテキストの共有管理
 *
 * 複数チャネルで同じコンテキストを共有するため。
 */
class ZMQContext {
public:
    /**
     * @brief シングルトンインスタンス取得
     */
    static ZMQContext& Instance();

    /**
     * @brief コンテキスト取得
     */
    zmq::context_t& Get() { return *context_; }

    // コピー禁止
    ZMQContext(const ZMQContext&) = delete;
    ZMQContext& operator=(const ZMQContext&) = delete;

private:
    ZMQContext();
    ~ZMQContext() = default;

    std::unique_ptr<zmq::context_t> context_;
};

}  // namespace DELILA::Net
```

---

### P2-4: ZMQDataChannel クラス作成
**見積もり**: 1.5時間

```cpp
// lib/net/include/infrastructure/zmq/ZMQDataChannel.hpp
#pragma once

#include "interfaces/IDataChannel.hpp"
#include <memory>
#include <string>
#include <zmq.hpp>

namespace DELILA::Net {

/**
 * @brief データチャネル設定
 */
struct DataChannelConfig {
    std::string address = "tcp://localhost:5555";
    std::string pattern = "PUB";  // PUB, SUB, PUSH, PULL
    bool bind = true;
    int receive_timeout_ms = 1000;
};

/**
 * @brief ZMQ データチャネル実装
 */
class ZMQDataChannel : public IDataChannel {
public:
    explicit ZMQDataChannel(const DataChannelConfig& config);
    ~ZMQDataChannel() override;

    bool Connect() override;
    void Disconnect() override;
    bool IsConnected() const override { return connected_; }

    bool Send(std::span<const uint8_t> data) override;
    std::optional<std::vector<uint8_t>> Receive() override;
    void SetReceiveTimeout(int timeout_ms) override;

private:
    DataChannelConfig config_;
    std::unique_ptr<zmq::socket_t> socket_;
    bool connected_ = false;

    int GetSocketType() const;
    bool IsSendPattern() const;
};

}  // namespace DELILA::Net
```

**テスト項目**:
- [ ] PUB/SUB パターンで送受信
- [ ] PUSH/PULL パターンで送受信
- [ ] 接続/切断が正しく動作
- [ ] タイムアウトが正しく動作

---

### P2-5: ZMQStatusChannel クラス作成
**見積もり**: 1時間

```cpp
// lib/net/include/infrastructure/zmq/ZMQStatusChannel.hpp
#pragma once

#include "interfaces/IStatusChannel.hpp"
#include "domain/ComponentStatus.hpp"
#include <memory>
#include <string>
#include <zmq.hpp>

namespace DELILA::Net {

/**
 * @brief ステータスチャネル設定
 */
struct StatusChannelConfig {
    std::string address = "tcp://localhost:5556";
    bool bind = true;
    int receive_timeout_ms = 1000;
};

/**
 * @brief ZMQ ステータスチャネル実装
 *
 * PUB/SUB パターンを使用
 */
class ZMQStatusChannel : public IStatusChannel {
public:
    explicit ZMQStatusChannel(const StatusChannelConfig& config, bool is_publisher);
    ~ZMQStatusChannel() override;

    bool Connect() override;
    void Disconnect() override;
    bool IsConnected() const override { return connected_; }

    bool SendStatus(const ComponentStatus& status) override;
    std::optional<ComponentStatus> ReceiveStatus() override;

private:
    StatusChannelConfig config_;
    bool is_publisher_;
    std::unique_ptr<zmq::socket_t> socket_;
    bool connected_ = false;

    // JSON シリアライズ（内部実装）
    std::string SerializeToJson(const ComponentStatus& status) const;
    std::optional<ComponentStatus> DeserializeFromJson(const std::string& json) const;
};

}  // namespace DELILA::Net
```

**テスト項目**:
- [ ] ステータス送受信が正しく動作
- [ ] JSON シリアライズ/デシリアライズが正しい

---

---

## テスト戦略

### 既存テストの削除

以下の既存テストは新しいテストに置き換えるため**削除**する:

| 削除対象 | 理由 |
|---------|------|
| `tests/unit/net/test_serializer_format_versions.cpp` | 新 BinarySerializer テストで置き換え |
| `tests/unit/net/test_data_processor.cpp` | Phase 5 でFacade用テストに置き換え |
| `tests/unit/net/test_data_processor_sequence.cpp` | Phase 5 でFacade用テストに置き換え |
| `tests/unit/net/test_zmq_transport_bytes.cpp` | 新 ZMQDataChannel テストで置き換え |
| `tests/unit/net/test_zmq_transport_minimal.cpp` | 新チャネルテストで置き換え |

### 新規テスト作成

**ファイル**: `tests/unit/net/infrastructure/test_crc32_checksum.cpp`

```cpp
// tests/unit/net/infrastructure/test_crc32_checksum.cpp
#include <gtest/gtest.h>
#include "infrastructure/checksum/CRC32Checksum.hpp"

namespace DELILA::Net {

TEST(CRC32ChecksumTest, KnownValue) {
    CRC32Checksum checksum;
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    uint32_t crc = checksum.Calculate(data);
    EXPECT_TRUE(checksum.Verify(data, crc));
}

TEST(CRC32ChecksumTest, EmptyData) {
    CRC32Checksum checksum;
    std::vector<uint8_t> empty;
    uint32_t crc = checksum.Calculate(empty);
    EXPECT_TRUE(checksum.Verify(empty, crc));
}

TEST(CRC32ChecksumTest, VerifyFails) {
    CRC32Checksum checksum;
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    uint32_t crc = checksum.Calculate(data);
    EXPECT_FALSE(checksum.Verify(data, crc + 1));  // 不正な CRC
}

TEST(CRC32ChecksumTest, TypeId) {
    CRC32Checksum checksum;
    EXPECT_EQ(checksum.GetTypeId(), 1);  // CHECKSUM_CRC32
}

TEST(CRC32ChecksumTest, DifferentDataDifferentCRC) {
    CRC32Checksum checksum;
    std::vector<uint8_t> data1 = {1, 2, 3};
    std::vector<uint8_t> data2 = {1, 2, 4};
    EXPECT_NE(checksum.Calculate(data1), checksum.Calculate(data2));
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/infrastructure/test_binary_serializer.cpp`

```cpp
// tests/unit/net/infrastructure/test_binary_serializer.cpp
#include <gtest/gtest.h>
#include "infrastructure/serialization/BinarySerializer.hpp"

namespace DELILA::Net {

TEST(EventDataSerializerTest, RoundTrip) {
    EventDataSerializer serializer;

    std::vector<std::unique_ptr<DELILA::Digitizer::EventData>> events;
    auto event = std::make_unique<DELILA::Digitizer::EventData>();
    event->timeStampNs = 1234567890;
    event->energy = 1000;
    event->module = 1;
    event->channel = 5;
    events.push_back(std::move(event));

    auto serialized = serializer.Serialize(events);
    EXPECT_FALSE(serialized.empty());

    auto deserialized = serializer.Deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    ASSERT_EQ(deserialized->size(), 1);
    EXPECT_EQ((*deserialized)[0]->timeStampNs, 1234567890);
    EXPECT_EQ((*deserialized)[0]->energy, 1000);
}

TEST(EventDataSerializerTest, EmptyData) {
    EventDataSerializer serializer;
    std::vector<std::unique_ptr<DELILA::Digitizer::EventData>> empty;
    auto serialized = serializer.Serialize(empty);
    EXPECT_TRUE(serialized.empty());
}

TEST(MinimalEventDataSerializerTest, RoundTrip) {
    MinimalEventDataSerializer serializer;

    std::vector<std::unique_ptr<DELILA::Digitizer::MinimalEventData>> events;
    auto event = std::make_unique<DELILA::Digitizer::MinimalEventData>();
    event->timestamp = 9876543210ULL;
    event->energy = 500;
    event->extras = 42;
    events.push_back(std::move(event));

    auto serialized = serializer.Serialize(events);
    EXPECT_FALSE(serialized.empty());
    EXPECT_EQ(serialized.size(), MinimalEventDataSerializer::RECORD_SIZE);

    auto deserialized = serializer.Deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    ASSERT_EQ(deserialized->size(), 1);
    EXPECT_EQ((*deserialized)[0]->timestamp, 9876543210ULL);
    EXPECT_EQ((*deserialized)[0]->energy, 500);
}

TEST(MinimalEventDataSerializerTest, InvalidData) {
    MinimalEventDataSerializer serializer;
    std::vector<uint8_t> invalid = {1, 2, 3};  // 不完全なデータ
    auto result = serializer.Deserialize(invalid);
    EXPECT_FALSE(result.has_value());
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/infrastructure/test_zmq_data_channel.cpp`

```cpp
// tests/unit/net/infrastructure/test_zmq_data_channel.cpp
#include <gtest/gtest.h>
#include "infrastructure/zmq/ZMQDataChannel.hpp"
#include <thread>

namespace DELILA::Net {

class ZMQDataChannelTest : public ::testing::Test {
protected:
    void TearDown() override {
        // ポートの再利用を防ぐため少し待機
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

TEST_F(ZMQDataChannelTest, PushPullPattern) {
    DataChannelConfig push_config{"tcp://127.0.0.1:15555", "PUSH", true};
    DataChannelConfig pull_config{"tcp://127.0.0.1:15555", "PULL", false};

    ZMQDataChannel pusher(push_config);
    ZMQDataChannel puller(pull_config);

    ASSERT_TRUE(pusher.Connect());
    ASSERT_TRUE(puller.Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    EXPECT_TRUE(pusher.Send(data));

    auto received = puller.Receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, data);
}

TEST_F(ZMQDataChannelTest, PubSubPattern) {
    DataChannelConfig pub_config{"tcp://127.0.0.1:15556", "PUB", true};
    DataChannelConfig sub_config{"tcp://127.0.0.1:15556", "SUB", false};

    ZMQDataChannel publisher(pub_config);
    ZMQDataChannel subscriber(sub_config);

    ASSERT_TRUE(publisher.Connect());
    ASSERT_TRUE(subscriber.Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // SUB の接続待ち

    std::vector<uint8_t> data = {10, 20, 30};
    EXPECT_TRUE(publisher.Send(data));

    auto received = subscriber.Receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, data);
}

TEST_F(ZMQDataChannelTest, ReceiveTimeout) {
    DataChannelConfig pull_config{"tcp://127.0.0.1:15557", "PULL", true};

    ZMQDataChannel puller(pull_config);
    puller.SetReceiveTimeout(100);  // 100ms タイムアウト
    ASSERT_TRUE(puller.Connect());

    auto start = std::chrono::steady_clock::now();
    auto received = puller.Receive();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(received.has_value());
    EXPECT_LT(elapsed, std::chrono::milliseconds(500));  // タイムアウトが機能
}

TEST_F(ZMQDataChannelTest, ConnectDisconnect) {
    DataChannelConfig config{"tcp://127.0.0.1:15558", "PUSH", true};
    ZMQDataChannel channel(config);

    EXPECT_FALSE(channel.IsConnected());
    EXPECT_TRUE(channel.Connect());
    EXPECT_TRUE(channel.IsConnected());

    channel.Disconnect();
    EXPECT_FALSE(channel.IsConnected());
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/infrastructure/test_zmq_status_channel.cpp`

```cpp
// tests/unit/net/infrastructure/test_zmq_status_channel.cpp
#include <gtest/gtest.h>
#include "infrastructure/zmq/ZMQStatusChannel.hpp"
#include <thread>

namespace DELILA::Net {

class ZMQStatusChannelTest : public ::testing::Test {
protected:
    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

TEST_F(ZMQStatusChannelTest, SendReceiveStatus) {
    StatusChannelConfig pub_config{"tcp://127.0.0.1:15559", true};
    StatusChannelConfig sub_config{"tcp://127.0.0.1:15559", false};

    ZMQStatusChannel publisher(pub_config, true);   // PUB
    ZMQStatusChannel subscriber(sub_config, false); // SUB

    ASSERT_TRUE(publisher.Connect());
    ASSERT_TRUE(subscriber.Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ComponentStatus status;
    status.component_id = "digitizer_01";
    status.state = "Running";
    status.heartbeat_counter = 42;
    status.metrics["events_per_second"] = 1000.0;

    EXPECT_TRUE(publisher.SendStatus(status));

    auto received = subscriber.ReceiveStatus();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->component_id, "digitizer_01");
    EXPECT_EQ(received->state, "Running");
    EXPECT_EQ(received->heartbeat_counter, 42);
    EXPECT_DOUBLE_EQ(received->metrics["events_per_second"], 1000.0);
}

TEST_F(ZMQStatusChannelTest, StatusWithError) {
    StatusChannelConfig pub_config{"tcp://127.0.0.1:15560", true};
    StatusChannelConfig sub_config{"tcp://127.0.0.1:15560", false};

    ZMQStatusChannel publisher(pub_config, true);
    ZMQStatusChannel subscriber(sub_config, false);

    ASSERT_TRUE(publisher.Connect());
    ASSERT_TRUE(subscriber.Connect());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ComponentStatus status;
    status.component_id = "digitizer_01";
    status.state = "Error";
    status.error_message = "Hardware communication error";

    EXPECT_TRUE(publisher.SendStatus(status));

    auto received = subscriber.ReceiveStatus();
    ASSERT_TRUE(received.has_value());
    EXPECT_TRUE(received->IsError());
    EXPECT_EQ(received->error_message, "Hardware communication error");
}

}  // namespace DELILA::Net
```

### テストディレクトリ構造

```
tests/unit/net/
├── infrastructure/                        # 新規ディレクトリ
│   ├── test_crc32_checksum.cpp            # 新規作成
│   ├── test_binary_serializer.cpp         # 新規作成
│   ├── test_zmq_data_channel.cpp          # 新規作成
│   └── test_zmq_status_channel.cpp        # 新規作成
└── ... (既存ファイルは削除予定)
```

### CMakeLists.txt 更新

```cmake
# tests/unit/net/infrastructure のテストを追加
add_executable(delila_net_infrastructure_tests
    infrastructure/test_crc32_checksum.cpp
    infrastructure/test_binary_serializer.cpp
    infrastructure/test_zmq_data_channel.cpp
    infrastructure/test_zmq_status_channel.cpp
)

target_link_libraries(delila_net_infrastructure_tests
    delila_net
    delila_core
    GTest::gtest_main
)

add_test(NAME NetInfrastructureTests COMMAND delila_net_infrastructure_tests)
```

---

## 完了条件

- [ ] CRC32Checksum が IChecksum を実装
- [ ] BinarySerializer が ISerializer を実装
- [ ] ZMQDataChannel が IDataChannel を実装
- [ ] ZMQStatusChannel が IStatusChannel を実装
- [ ] 既存テスト削除（上記テーブル参照）
- [ ] 新規 Infrastructure テスト作成
- [ ] 全ユニットテストがパス
- [ ] 既存の DataProcessor から LZ4 関連コードを削除
- [ ] CMakeLists.txt から lz4 依存を削除
- [ ] 既存の DataProcessor/ZMQTransport はまだ存在（Phase 5 で置換）

---

## 次のPhase

Phase 2 完了後、Phase 3（Infrastructure層 - Command通信）に進む。
