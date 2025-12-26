# Phase 1: ドメイン層

**見積もり**: 3時間
**目的**: 外部依存のないドメインオブジェクトの定義
**依存**: Phase 0 完了

---

## 設計原則

- **外部ライブラリ依存なし**: std のみ使用
- **シンプルな構造体**: ビジネスロジックは含まない
- **シリアライズ非依存**: JSON/バイナリ変換はInfrastructure層

---

## タスク一覧

### P1-1: ComponentStatus を domain/ に移動・分離
**見積もり**: 30分

```cpp
// lib/net/include/domain/ComponentStatus.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace DELILA::Net {

/**
 * @brief コンポーネントのステータス情報
 *
 * ZMQTransport.hpp から分離。nlohmann/json 依存を除去。
 */
struct ComponentStatus {
    std::string component_id;
    std::string state;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, double> metrics;
    std::string error_message;
    uint64_t heartbeat_counter = 0;

    // ヘルパーメソッド
    bool IsError() const { return !error_message.empty(); }
    void ClearError() { error_message.clear(); }
};

}  // namespace DELILA::Net
```

**完了条件**:
- [ ] 新ファイル作成
- [ ] nlohmann/json 依存なし
- [ ] ZMQTransport.hpp から削除（Phase 5で対応）

---

### P1-2: Command 構造体定義
**見積もり**: 30分

```cpp
// lib/net/include/domain/Command.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace DELILA::Net {

/**
 * @brief コマンドタイプ
 */
enum class CommandType : uint8_t {
    CONFIGURE = 1,
    ARM = 2,
    START = 3,
    STOP = 4,
    STATUS = 5,
    RESET = 6,
    CUSTOM = 99
};

/**
 * @brief コマンドタイプを文字列に変換
 */
inline std::string CommandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::CONFIGURE: return "CONFIGURE";
        case CommandType::ARM: return "ARM";
        case CommandType::START: return "START";
        case CommandType::STOP: return "STOP";
        case CommandType::STATUS: return "STATUS";
        case CommandType::RESET: return "RESET";
        case CommandType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 文字列からコマンドタイプに変換
 */
inline CommandType StringToCommandType(const std::string& str) {
    if (str == "CONFIGURE") return CommandType::CONFIGURE;
    if (str == "ARM") return CommandType::ARM;
    if (str == "START") return CommandType::START;
    if (str == "STOP") return CommandType::STOP;
    if (str == "STATUS") return CommandType::STATUS;
    if (str == "RESET") return CommandType::RESET;
    if (str == "CUSTOM") return CommandType::CUSTOM;
    return CommandType::CUSTOM;  // デフォルト
}

/**
 * @brief コマンドパラメータの型
 *
 * JSON依存を避けるため、基本型のvariantを使用
 */
using ParamValue = std::variant<
    bool,
    int64_t,
    double,
    std::string,
    std::vector<std::string>,
    std::map<std::string, std::string>
>;

/**
 * @brief Operatorからコンポーネントへのコマンド
 */
struct Command {
    CommandType type = CommandType::STATUS;
    std::string target_id;           // コンポーネントID
    std::string group_id;            // オプション: グループID
    uint64_t request_id = 0;         // リクエスト追跡用
    uint32_t timeout_ms = 5000;      // タイムアウト（ミリ秒）
    std::map<std::string, ParamValue> params;  // パラメータ
    std::chrono::system_clock::time_point timestamp;

    // コンストラクタ
    Command() : timestamp(std::chrono::system_clock::now()) {}

    explicit Command(CommandType t, const std::string& target = "")
        : type(t), target_id(target), timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief コマンドごとのデフォルトタイムアウト
 */
inline uint32_t GetDefaultTimeout(CommandType type) {
    switch (type) {
        case CommandType::CONFIGURE: return 30000;  // 30秒
        case CommandType::ARM: return 10000;        // 10秒
        case CommandType::START: return 5000;       // 5秒
        case CommandType::STOP: return 10000;       // 10秒
        case CommandType::STATUS: return 2000;      // 2秒
        case CommandType::RESET: return 10000;      // 10秒
        case CommandType::CUSTOM: return 30000;     // 30秒
        default: return 5000;
    }
}

}  // namespace DELILA::Net
```

**テスト項目**:
- [ ] CommandType 変換が正しく動作
- [ ] デフォルトタイムアウト値が仕様通り

---

### P1-3: CommandResponse 構造体定義
**見積もり**: 30分

```cpp
// lib/net/include/domain/CommandResponse.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "ComponentState.hpp"
#include "ErrorCode.hpp"

namespace DELILA::Net {

/**
 * @brief コマンドへのレスポンス
 */
struct CommandResponse {
    uint64_t request_id = 0;              // 対応するリクエストID
    bool success = false;                  // 成功/失敗
    ComponentState current_state = ComponentState::Loaded;  // 現在の状態
    ErrorCode error_code = ErrorCode::SUCCESS;              // エラーコード
    std::string error_message;             // エラー詳細
    std::chrono::system_clock::time_point timestamp;

    // コンストラクタ
    CommandResponse() : timestamp(std::chrono::system_clock::now()) {}

    // 成功レスポンス生成
    static CommandResponse Success(uint64_t req_id, ComponentState state) {
        CommandResponse resp;
        resp.request_id = req_id;
        resp.success = true;
        resp.current_state = state;
        resp.error_code = ErrorCode::SUCCESS;
        return resp;
    }

    // 失敗レスポンス生成
    static CommandResponse Failure(uint64_t req_id,
                                   ComponentState state,
                                   ErrorCode code,
                                   const std::string& message = "") {
        CommandResponse resp;
        resp.request_id = req_id;
        resp.success = false;
        resp.current_state = state;
        resp.error_code = code;
        resp.error_message = message;
        return resp;
    }
};

}  // namespace DELILA::Net
```

---

### P1-4: ErrorCode enum 定義
**見積もり**: 30分

```cpp
// lib/net/include/domain/ErrorCode.hpp
#pragma once

#include <cstdint>
#include <string>

namespace DELILA::Net {

/**
 * @brief エラーカテゴリ
 */
enum class ErrorCategory : uint16_t {
    NONE = 0x0000,
    NETWORK = 0x1000,
    HARDWARE = 0x2000,
    CONFIG = 0x3000,
    STATE = 0x4000,
    DATA = 0x5000,
    TIMEOUT = 0x6000,
    INTERNAL = 0x7000,
};

/**
 * @brief エラーコード
 */
enum class ErrorCode : uint16_t {
    // 成功
    SUCCESS = 0x0000,

    // ネットワークエラー (0x1000-0x1FFF)
    NETWORK_CONNECTION_FAILED = 0x1001,
    NETWORK_SEND_FAILED = 0x1002,
    NETWORK_RECEIVE_FAILED = 0x1003,
    NETWORK_TIMEOUT = 0x1004,
    NETWORK_DISCONNECTED = 0x1005,

    // ハードウェアエラー (0x2000-0x2FFF)
    HARDWARE_NOT_FOUND = 0x2001,
    HARDWARE_INIT_FAILED = 0x2002,
    HARDWARE_COMM_ERROR = 0x2003,
    HARDWARE_BUSY = 0x2004,

    // 設定エラー (0x3000-0x3FFF)
    CONFIG_INVALID = 0x3001,
    CONFIG_MISSING_PARAM = 0x3002,
    CONFIG_OUT_OF_RANGE = 0x3003,

    // 状態遷移エラー (0x4000-0x4FFF)
    STATE_INVALID_TRANSITION = 0x4001,
    STATE_NOT_CONFIGURED = 0x4002,
    STATE_NOT_ARMED = 0x4003,
    STATE_ALREADY_RUNNING = 0x4004,

    // データエラー (0x5000-0x5FFF)
    DATA_CORRUPTED = 0x5001,
    DATA_OVERFLOW = 0x5002,
    DATA_SEQUENCE_GAP = 0x5003,

    // タイムアウト (0x6000-0x6FFF)
    TIMEOUT_COMMAND = 0x6001,
    TIMEOUT_RESPONSE = 0x6002,
    TIMEOUT_DATA = 0x6003,

    // 内部エラー (0x7000-0x7FFF)
    INTERNAL_ERROR = 0x7001,
    INTERNAL_NOT_IMPLEMENTED = 0x7002,
};

/**
 * @brief エラーコードからカテゴリを取得
 */
inline ErrorCategory GetErrorCategory(ErrorCode code) {
    uint16_t category = static_cast<uint16_t>(code) & 0xF000;
    return static_cast<ErrorCategory>(category);
}

/**
 * @brief エラーコードを文字列に変換
 */
inline std::string ErrorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::NETWORK_CONNECTION_FAILED: return "NETWORK_CONNECTION_FAILED";
        case ErrorCode::NETWORK_SEND_FAILED: return "NETWORK_SEND_FAILED";
        case ErrorCode::NETWORK_RECEIVE_FAILED: return "NETWORK_RECEIVE_FAILED";
        case ErrorCode::NETWORK_TIMEOUT: return "NETWORK_TIMEOUT";
        case ErrorCode::NETWORK_DISCONNECTED: return "NETWORK_DISCONNECTED";
        case ErrorCode::HARDWARE_NOT_FOUND: return "HARDWARE_NOT_FOUND";
        case ErrorCode::HARDWARE_INIT_FAILED: return "HARDWARE_INIT_FAILED";
        case ErrorCode::HARDWARE_COMM_ERROR: return "HARDWARE_COMM_ERROR";
        case ErrorCode::HARDWARE_BUSY: return "HARDWARE_BUSY";
        case ErrorCode::CONFIG_INVALID: return "CONFIG_INVALID";
        case ErrorCode::CONFIG_MISSING_PARAM: return "CONFIG_MISSING_PARAM";
        case ErrorCode::CONFIG_OUT_OF_RANGE: return "CONFIG_OUT_OF_RANGE";
        case ErrorCode::STATE_INVALID_TRANSITION: return "STATE_INVALID_TRANSITION";
        case ErrorCode::STATE_NOT_CONFIGURED: return "STATE_NOT_CONFIGURED";
        case ErrorCode::STATE_NOT_ARMED: return "STATE_NOT_ARMED";
        case ErrorCode::STATE_ALREADY_RUNNING: return "STATE_ALREADY_RUNNING";
        case ErrorCode::DATA_CORRUPTED: return "DATA_CORRUPTED";
        case ErrorCode::DATA_OVERFLOW: return "DATA_OVERFLOW";
        case ErrorCode::DATA_SEQUENCE_GAP: return "DATA_SEQUENCE_GAP";
        case ErrorCode::TIMEOUT_COMMAND: return "TIMEOUT_COMMAND";
        case ErrorCode::TIMEOUT_RESPONSE: return "TIMEOUT_RESPONSE";
        case ErrorCode::TIMEOUT_DATA: return "TIMEOUT_DATA";
        case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
        case ErrorCode::INTERNAL_NOT_IMPLEMENTED: return "INTERNAL_NOT_IMPLEMENTED";
        default: return "UNKNOWN_ERROR";
    }
}

/**
 * @brief エラーかどうかを判定
 */
inline bool IsError(ErrorCode code) {
    return code != ErrorCode::SUCCESS;
}

}  // namespace DELILA::Net
```

---

### P1-5: Metrics 構造体定義
**見積もり**: 30分

```cpp
// lib/net/include/domain/Metrics.hpp
#pragma once

#include <chrono>
#include <cstdint>

namespace DELILA::Net {

/**
 * @brief パフォーマンスメトリクス
 */
struct Metrics {
    // スループット
    double events_per_second = 0.0;
    double bytes_per_second = 0.0;

    // バッファ
    uint64_t queue_size = 0;
    uint64_t queue_capacity = 0;
    uint64_t dropped_events = 0;

    // レイテンシ
    double avg_latency_us = 0.0;
    double max_latency_us = 0.0;

    // エラー
    uint64_t error_count = 0;

    // 稼働
    uint64_t uptime_seconds = 0;

    // シーケンス
    uint64_t sequence_gaps = 0;

    // タイムスタンプ
    std::chrono::system_clock::time_point timestamp;

    // コンストラクタ
    Metrics() : timestamp(std::chrono::system_clock::now()) {}

    // リセット
    void Reset() {
        events_per_second = 0.0;
        bytes_per_second = 0.0;
        queue_size = 0;
        // queue_capacity は維持
        dropped_events = 0;
        avg_latency_us = 0.0;
        max_latency_us = 0.0;
        error_count = 0;
        uptime_seconds = 0;
        sequence_gaps = 0;
        timestamp = std::chrono::system_clock::now();
    }
};

}  // namespace DELILA::Net
```

---

### P1-6: ドメイン層ユニットテスト
**見積もり**: 30分

```cpp
// tests/unit/net/domain/test_domain_objects.cpp

#include <gtest/gtest.h>
#include "domain/Command.hpp"
#include "domain/CommandResponse.hpp"
#include "domain/ErrorCode.hpp"
#include "domain/Metrics.hpp"
#include "domain/ComponentStatus.hpp"

namespace DELILA::Net {

// CommandType テスト
TEST(CommandTypeTest, ConvertToString) {
    EXPECT_EQ(CommandTypeToString(CommandType::CONFIGURE), "CONFIGURE");
    EXPECT_EQ(CommandTypeToString(CommandType::ARM), "ARM");
    EXPECT_EQ(CommandTypeToString(CommandType::START), "START");
    EXPECT_EQ(CommandTypeToString(CommandType::STOP), "STOP");
}

TEST(CommandTypeTest, ConvertFromString) {
    EXPECT_EQ(StringToCommandType("CONFIGURE"), CommandType::CONFIGURE);
    EXPECT_EQ(StringToCommandType("ARM"), CommandType::ARM);
    EXPECT_EQ(StringToCommandType("UNKNOWN"), CommandType::CUSTOM);
}

TEST(CommandTypeTest, DefaultTimeout) {
    EXPECT_EQ(GetDefaultTimeout(CommandType::CONFIGURE), 30000);
    EXPECT_EQ(GetDefaultTimeout(CommandType::STATUS), 2000);
}

// ErrorCode テスト
TEST(ErrorCodeTest, GetCategory) {
    EXPECT_EQ(GetErrorCategory(ErrorCode::NETWORK_TIMEOUT), ErrorCategory::NETWORK);
    EXPECT_EQ(GetErrorCategory(ErrorCode::HARDWARE_BUSY), ErrorCategory::HARDWARE);
    EXPECT_EQ(GetErrorCategory(ErrorCode::SUCCESS), ErrorCategory::NONE);
}

TEST(ErrorCodeTest, IsError) {
    EXPECT_FALSE(IsError(ErrorCode::SUCCESS));
    EXPECT_TRUE(IsError(ErrorCode::NETWORK_TIMEOUT));
}

// CommandResponse テスト
TEST(CommandResponseTest, CreateSuccess) {
    auto resp = CommandResponse::Success(123, ComponentState::Armed);
    EXPECT_EQ(resp.request_id, 123);
    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.current_state, ComponentState::Armed);
    EXPECT_EQ(resp.error_code, ErrorCode::SUCCESS);
}

TEST(CommandResponseTest, CreateFailure) {
    auto resp = CommandResponse::Failure(456, ComponentState::Error,
                                         ErrorCode::STATE_INVALID_TRANSITION,
                                         "Cannot transition");
    EXPECT_EQ(resp.request_id, 456);
    EXPECT_FALSE(resp.success);
    EXPECT_EQ(resp.error_code, ErrorCode::STATE_INVALID_TRANSITION);
    EXPECT_EQ(resp.error_message, "Cannot transition");
}

// Metrics テスト
TEST(MetricsTest, DefaultValues) {
    Metrics m;
    EXPECT_DOUBLE_EQ(m.events_per_second, 0.0);
    EXPECT_EQ(m.queue_size, 0);
}

TEST(MetricsTest, Reset) {
    Metrics m;
    m.events_per_second = 100.0;
    m.error_count = 5;
    m.Reset();
    EXPECT_DOUBLE_EQ(m.events_per_second, 0.0);
    EXPECT_EQ(m.error_count, 0);
}

}  // namespace DELILA::Net
```

---

---

## テスト戦略

### 既存テストの削除

以下の既存テストは新しいテストに置き換えるため**削除**する:

| 削除対象 | 理由 |
|---------|------|
| `tests/unit/net/test_message_type.cpp` | MessageType は新アーキテクチャで不要 |

**注意**: Phase1 で定義するドメインオブジェクトの一部（Command, CommandResponse, ErrorCode）は既に Phase 0-Core で lib/core に移動している。
このPhaseでは lib/net 固有のドメイン（ComponentStatus, Metrics, DataHeader）のみを扱う。

### 新規テスト作成

**ファイル**: `tests/unit/net/domain/test_component_status.cpp`

```cpp
// tests/unit/net/domain/test_component_status.cpp
#include <gtest/gtest.h>
#include "domain/ComponentStatus.hpp"

namespace DELILA::Net {

TEST(ComponentStatusTest, DefaultConstruction) {
    ComponentStatus status;
    EXPECT_TRUE(status.component_id.empty());
    EXPECT_TRUE(status.state.empty());
    EXPECT_EQ(status.heartbeat_counter, 0);
}

TEST(ComponentStatusTest, IsError) {
    ComponentStatus status;
    EXPECT_FALSE(status.IsError());

    status.error_message = "Test error";
    EXPECT_TRUE(status.IsError());
}

TEST(ComponentStatusTest, ClearError) {
    ComponentStatus status;
    status.error_message = "Test error";
    EXPECT_TRUE(status.IsError());

    status.ClearError();
    EXPECT_FALSE(status.IsError());
}

TEST(ComponentStatusTest, MetricsMap) {
    ComponentStatus status;
    status.metrics["events_per_second"] = 1000.0;
    status.metrics["bytes_per_second"] = 1024000.0;

    EXPECT_DOUBLE_EQ(status.metrics["events_per_second"], 1000.0);
    EXPECT_EQ(status.metrics.size(), 2);
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/domain/test_metrics.cpp`

```cpp
// tests/unit/net/domain/test_metrics.cpp
#include <gtest/gtest.h>
#include "domain/Metrics.hpp"

namespace DELILA::Net {

TEST(MetricsTest, DefaultValues) {
    Metrics m;
    EXPECT_DOUBLE_EQ(m.events_per_second, 0.0);
    EXPECT_DOUBLE_EQ(m.bytes_per_second, 0.0);
    EXPECT_EQ(m.queue_size, 0);
    EXPECT_EQ(m.error_count, 0);
    EXPECT_EQ(m.sequence_gaps, 0);
}

TEST(MetricsTest, Reset) {
    Metrics m;
    m.events_per_second = 100.0;
    m.error_count = 5;
    m.max_latency_us = 500.0;
    m.queue_capacity = 1000;

    m.Reset();

    EXPECT_DOUBLE_EQ(m.events_per_second, 0.0);
    EXPECT_EQ(m.error_count, 0);
    EXPECT_DOUBLE_EQ(m.max_latency_us, 0.0);
    // queue_capacity は維持される
}

TEST(MetricsTest, TimestampSet) {
    Metrics m;
    auto now = std::chrono::system_clock::now();
    // タイムスタンプがコンストラクタで設定される
    EXPECT_LE(m.timestamp, now);
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/domain/test_data_header.cpp`

```cpp
// tests/unit/net/domain/test_data_header.cpp
#include <gtest/gtest.h>
#include "domain/DataHeader.hpp"

namespace DELILA::Net {

TEST(DataHeaderTest, ConstantValues) {
    // ヘッダーサイズが64バイトであることを確認
    EXPECT_EQ(sizeof(DataHeader), 64);
}

TEST(DataHeaderTest, MagicNumber) {
    EXPECT_EQ(DATA_MAGIC_NUMBER, 0x44454C494C413200ULL);  // "DELILA2\0"
}

TEST(DataHeaderTest, DefaultConstruction) {
    DataHeader header{};
    EXPECT_EQ(header.magic_number, 0);
    EXPECT_EQ(header.sequence_number, 0);
    EXPECT_EQ(header.event_count, 0);
}

}  // namespace DELILA::Net
```

### テストディレクトリ構造

```
tests/unit/net/
├── domain/                           # 新規ディレクトリ
│   ├── test_component_status.cpp     # 新規作成
│   ├── test_metrics.cpp              # 新規作成
│   └── test_data_header.cpp          # 新規作成
└── ... (既存ファイル)
```

### CMakeLists.txt 更新

```cmake
# tests/unit/net/domain のテストを追加
add_executable(delila_net_domain_tests
    domain/test_component_status.cpp
    domain/test_metrics.cpp
    domain/test_data_header.cpp
)

target_link_libraries(delila_net_domain_tests
    delila_net
    delila_core
    GTest::gtest_main
)

add_test(NAME NetDomainTests COMMAND delila_net_domain_tests)
```

---

## 完了条件

- [ ] 全ドメインオブジェクトが定義されている
- [ ] 外部ライブラリ依存がない（std のみ）
- [ ] 既存テスト削除（test_message_type.cpp）
- [ ] 新規ドメインテスト作成
- [ ] 全ユニットテストがパス

---

## 次のPhase

Phase 1 完了後、Phase 2（Infrastructure層 - 分離）に進む。
