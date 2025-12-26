# Phase 0-Core: lib/core 共通ドメイン層

**見積もり**: 2時間
**目的**: lib/net と lib/digitizer で共有するドメインオブジェクトを分離
**依存**: なし（最初に実行）

---

## 設計原則

- **外部依存なし**: 標準ライブラリのみ使用
- **共通概念**: DAQシステム全体で使用される概念を定義
- **シンプル**: 過度な抽象化を避ける

---

## ディレクトリ構造

```
lib/core/
├── include/
│   └── delila/
│       └── core/
│           ├── ComponentState.hpp    # 状態機械
│           ├── ComponentStatus.hpp   # ステータス情報
│           ├── Command.hpp           # コマンド定義
│           ├── CommandResponse.hpp   # レスポンス定義
│           ├── ErrorCode.hpp         # エラーコード
│           ├── EventData.hpp         # 既存（移動）
│           └── MinimalEventData.hpp  # 既存（移動）
└── CMakeLists.txt
```

---

## タスク一覧

### PC-1: ディレクトリ構造作成
**見積もり**: 15分

```bash
mkdir -p lib/core/include/delila/core
```

---

### PC-2: ComponentState 移動・整理
**見積もり**: 15分

```cpp
// lib/core/include/delila/core/ComponentState.hpp
#pragma once

#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief DAQコンポーネントのライフサイクル状態
 *
 * 状態遷移:
 *   Loaded -> Configured -> Armed -> Running <-> Paused
 *                                      |
 *                                      v
 *                                   Loaded (stop)
 */
enum class ComponentState : uint8_t {
    Loaded,      ///< 初期状態
    Configured,  ///< 設定完了
    Armed,       ///< 開始準備完了（2-phase start用）
    Running,     ///< データ収集中
    Paused,      ///< 一時停止
    Error        ///< エラー状態
};

/**
 * @brief ComponentState を文字列に変換
 */
inline std::string ComponentStateToString(ComponentState state) {
    switch (state) {
        case ComponentState::Loaded:     return "Loaded";
        case ComponentState::Configured: return "Configured";
        case ComponentState::Armed:      return "Armed";
        case ComponentState::Running:    return "Running";
        case ComponentState::Paused:     return "Paused";
        case ComponentState::Error:      return "Error";
        default:                         return "Unknown";
    }
}

/**
 * @brief 状態遷移が有効かチェック
 */
inline bool IsValidTransition(ComponentState from, ComponentState to) {
    if (from == to) return false;

    // どの状態からも Loaded（リセット）と Error に遷移可能
    if (to == ComponentState::Loaded || to == ComponentState::Error) {
        return true;
    }

    switch (from) {
        case ComponentState::Loaded:     return to == ComponentState::Configured;
        case ComponentState::Configured: return to == ComponentState::Armed;
        case ComponentState::Armed:      return to == ComponentState::Running;
        case ComponentState::Running:    return to == ComponentState::Paused;
        case ComponentState::Paused:     return to == ComponentState::Running;
        case ComponentState::Error:      return false;  // Error からは Loaded のみ
        default:                         return false;
    }
}

}  // namespace DELILA
```

---

### PC-3: ComponentStatus 定義
**見積もり**: 15分

```cpp
// lib/core/include/delila/core/ComponentStatus.hpp
#pragma once

#include "ComponentState.hpp"
#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief コンポーネントのステータス情報
 *
 * ネットワーク経由で送受信される状態情報。
 */
struct ComponentStatus {
    std::string component_id;           ///< コンポーネント識別子
    ComponentState state = ComponentState::Loaded;
    uint64_t timestamp_ns = 0;          ///< タイムスタンプ（ナノ秒）
    uint64_t events_processed = 0;      ///< 処理済みイベント数
    uint64_t bytes_transferred = 0;     ///< 転送バイト数
    std::string error_message;          ///< エラーメッセージ（エラー時のみ）
    uint32_t error_code = 0;            ///< エラーコード
};

}  // namespace DELILA
```

---

### PC-4: Command / CommandResponse 定義
**見積もり**: 30分

```cpp
// lib/core/include/delila/core/Command.hpp
#pragma once

#include "ComponentState.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace DELILA {

/**
 * @brief コマンドタイプ
 */
enum class CommandType : uint8_t {
    CONFIGURE,  ///< 設定適用
    ARM,        ///< 開始準備
    START,      ///< データ収集開始
    STOP,       ///< データ収集停止
    PAUSE,      ///< 一時停止
    RESUME,     ///< 再開
    RESET,      ///< リセット
    STATUS,     ///< ステータス取得
    SHUTDOWN    ///< シャットダウン
};

inline std::string CommandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::CONFIGURE: return "CONFIGURE";
        case CommandType::ARM:       return "ARM";
        case CommandType::START:     return "START";
        case CommandType::STOP:      return "STOP";
        case CommandType::PAUSE:     return "PAUSE";
        case CommandType::RESUME:    return "RESUME";
        case CommandType::RESET:     return "RESET";
        case CommandType::STATUS:    return "STATUS";
        case CommandType::SHUTDOWN:  return "SHUTDOWN";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief コマンド
 */
struct Command {
    CommandType type;
    std::string target_component;       ///< 対象コンポーネントID（空=全体）
    uint64_t request_id = 0;            ///< リクエスト識別子
    std::unordered_map<std::string, std::string> parameters;  ///< 追加パラメータ

    Command() = default;
    Command(CommandType t, const std::string& target = "")
        : type(t), target_component(target) {}
};

}  // namespace DELILA
```

```cpp
// lib/core/include/delila/core/CommandResponse.hpp
#pragma once

#include "ComponentState.hpp"
#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief コマンドレスポンス
 */
struct CommandResponse {
    uint64_t request_id = 0;            ///< 対応するリクエストID
    bool success = false;               ///< 成功/失敗
    ComponentState current_state = ComponentState::Loaded;  ///< 現在の状態
    uint32_t error_code = 0;            ///< エラーコード（失敗時）
    std::string error_message;          ///< エラーメッセージ（失敗時）

    static CommandResponse Success(uint64_t req_id, ComponentState state) {
        CommandResponse resp;
        resp.request_id = req_id;
        resp.success = true;
        resp.current_state = state;
        return resp;
    }

    static CommandResponse Failure(uint64_t req_id, ComponentState state,
                                   uint32_t code, const std::string& msg) {
        CommandResponse resp;
        resp.request_id = req_id;
        resp.success = false;
        resp.current_state = state;
        resp.error_code = code;
        resp.error_message = msg;
        return resp;
    }
};

}  // namespace DELILA
```

---

### PC-5: ErrorCode 定義
**見積もり**: 15分

```cpp
// lib/core/include/delila/core/ErrorCode.hpp
#pragma once

#include <cstdint>
#include <string>

namespace DELILA {

/**
 * @brief エラーコード
 */
enum class ErrorCode : uint32_t {
    SUCCESS = 0,

    // 一般エラー (1-99)
    UNKNOWN_ERROR = 1,
    INVALID_PARAMETER = 2,
    INVALID_STATE = 3,
    TIMEOUT = 4,

    // 通信エラー (100-199)
    CONNECTION_FAILED = 100,
    SEND_FAILED = 101,
    RECEIVE_FAILED = 102,
    SERIALIZATION_ERROR = 103,
    CHECKSUM_ERROR = 104,

    // ハードウェアエラー (200-299)
    HARDWARE_NOT_FOUND = 200,
    HARDWARE_INIT_FAILED = 201,
    HARDWARE_COMM_ERROR = 202,
    HARDWARE_CONFIG_ERROR = 203,

    // 状態遷移エラー (300-399)
    INVALID_TRANSITION = 300,
    NOT_CONFIGURED = 301,
    NOT_ARMED = 302,
    ALREADY_RUNNING = 303
};

inline std::string ErrorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS:              return "Success";
        case ErrorCode::UNKNOWN_ERROR:        return "Unknown error";
        case ErrorCode::INVALID_PARAMETER:    return "Invalid parameter";
        case ErrorCode::INVALID_STATE:        return "Invalid state";
        case ErrorCode::TIMEOUT:              return "Timeout";
        case ErrorCode::CONNECTION_FAILED:    return "Connection failed";
        case ErrorCode::SEND_FAILED:          return "Send failed";
        case ErrorCode::RECEIVE_FAILED:       return "Receive failed";
        case ErrorCode::SERIALIZATION_ERROR:  return "Serialization error";
        case ErrorCode::CHECKSUM_ERROR:       return "Checksum error";
        case ErrorCode::HARDWARE_NOT_FOUND:   return "Hardware not found";
        case ErrorCode::HARDWARE_INIT_FAILED: return "Hardware init failed";
        case ErrorCode::HARDWARE_COMM_ERROR:  return "Hardware comm error";
        case ErrorCode::HARDWARE_CONFIG_ERROR:return "Hardware config error";
        case ErrorCode::INVALID_TRANSITION:   return "Invalid transition";
        case ErrorCode::NOT_CONFIGURED:       return "Not configured";
        case ErrorCode::NOT_ARMED:            return "Not armed";
        case ErrorCode::ALREADY_RUNNING:      return "Already running";
        default:                              return "Unknown";
    }
}

}  // namespace DELILA
```

---

### PC-6: CMakeLists.txt 作成
**見積もり**: 15分

```cmake
# lib/core/CMakeLists.txt

cmake_minimum_required(VERSION 3.15)

# Header-only library
add_library(delila_core INTERFACE)

target_include_directories(delila_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# C++17 required
target_compile_features(delila_core INTERFACE cxx_std_17)
```

---

### PC-7: 既存コードの更新
**見積もり**: 15分

- `lib/net` の `ComponentState.hpp` を削除
- `lib/net` の `ComponentStatus` を `ZMQTransport.hpp` から削除
- 各ファイルで `#include <delila/core/ComponentState.hpp>` に変更

---

---

## テスト戦略

### 既存テストの削除

以下の既存テストは新しいテストに置き換えるため**削除**する:

| 削除対象 | 理由 |
|---------|------|
| `tests/unit/net/test_component_state.cpp` | lib/core に移動、namespace 変更 |

### 新規テスト作成

**ファイル**: `tests/unit/core/test_component_state.cpp`

```cpp
// tests/unit/core/test_component_state.cpp
#include <gtest/gtest.h>
#include <delila/core/ComponentState.hpp>

namespace DELILA {

// ComponentState 変換テスト
TEST(ComponentStateTest, ToStringConversion) {
    EXPECT_EQ(ComponentStateToString(ComponentState::Loaded), "Loaded");
    EXPECT_EQ(ComponentStateToString(ComponentState::Configured), "Configured");
    EXPECT_EQ(ComponentStateToString(ComponentState::Armed), "Armed");
    EXPECT_EQ(ComponentStateToString(ComponentState::Running), "Running");
    EXPECT_EQ(ComponentStateToString(ComponentState::Paused), "Paused");
    EXPECT_EQ(ComponentStateToString(ComponentState::Error), "Error");
}

// 有効な状態遷移テスト
TEST(ComponentStateTest, ValidTransitions) {
    EXPECT_TRUE(IsValidTransition(ComponentState::Loaded, ComponentState::Configured));
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Armed));
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Running));
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Paused));
    EXPECT_TRUE(IsValidTransition(ComponentState::Paused, ComponentState::Running));
}

// 無効な状態遷移テスト
TEST(ComponentStateTest, InvalidTransitions) {
    EXPECT_FALSE(IsValidTransition(ComponentState::Loaded, ComponentState::Loaded));
    EXPECT_FALSE(IsValidTransition(ComponentState::Loaded, ComponentState::Armed));
    EXPECT_FALSE(IsValidTransition(ComponentState::Loaded, ComponentState::Running));
    EXPECT_FALSE(IsValidTransition(ComponentState::Error, ComponentState::Running));
}

// どの状態からも Loaded/Error に遷移可能
TEST(ComponentStateTest, UniversalTransitions) {
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Loaded));
    EXPECT_TRUE(IsValidTransition(ComponentState::Running, ComponentState::Error));
    EXPECT_TRUE(IsValidTransition(ComponentState::Configured, ComponentState::Loaded));
    EXPECT_TRUE(IsValidTransition(ComponentState::Armed, ComponentState::Error));
}

}  // namespace DELILA
```

**ファイル**: `tests/unit/core/test_command.cpp`

```cpp
// tests/unit/core/test_command.cpp
#include <gtest/gtest.h>
#include <delila/core/Command.hpp>

namespace DELILA {

TEST(CommandTypeTest, ToStringConversion) {
    EXPECT_EQ(CommandTypeToString(CommandType::CONFIGURE), "CONFIGURE");
    EXPECT_EQ(CommandTypeToString(CommandType::ARM), "ARM");
    EXPECT_EQ(CommandTypeToString(CommandType::START), "START");
    EXPECT_EQ(CommandTypeToString(CommandType::STOP), "STOP");
    EXPECT_EQ(CommandTypeToString(CommandType::SHUTDOWN), "SHUTDOWN");
}

TEST(CommandTest, DefaultConstruction) {
    Command cmd;
    EXPECT_TRUE(cmd.target_component.empty());
    EXPECT_EQ(cmd.request_id, 0);
}

TEST(CommandTest, ConstructWithType) {
    Command cmd(CommandType::ARM, "digitizer_01");
    EXPECT_EQ(cmd.type, CommandType::ARM);
    EXPECT_EQ(cmd.target_component, "digitizer_01");
}

}  // namespace DELILA
```

**ファイル**: `tests/unit/core/test_error_code.cpp`

```cpp
// tests/unit/core/test_error_code.cpp
#include <gtest/gtest.h>
#include <delila/core/ErrorCode.hpp>

namespace DELILA {

TEST(ErrorCodeTest, ToStringConversion) {
    EXPECT_EQ(ErrorCodeToString(ErrorCode::SUCCESS), "Success");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::TIMEOUT), "Timeout");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::INVALID_STATE), "Invalid state");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::CHECKSUM_ERROR), "Checksum error");
}

TEST(ErrorCodeTest, SuccessIsZero) {
    EXPECT_EQ(static_cast<uint32_t>(ErrorCode::SUCCESS), 0);
}

}  // namespace DELILA
```

**ファイル**: `tests/unit/core/test_command_response.cpp`

```cpp
// tests/unit/core/test_command_response.cpp
#include <gtest/gtest.h>
#include <delila/core/CommandResponse.hpp>

namespace DELILA {

TEST(CommandResponseTest, CreateSuccess) {
    auto resp = CommandResponse::Success(123, ComponentState::Armed);
    EXPECT_EQ(resp.request_id, 123);
    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.current_state, ComponentState::Armed);
    EXPECT_EQ(resp.error_code, 0);
}

TEST(CommandResponseTest, CreateFailure) {
    auto resp = CommandResponse::Failure(456, ComponentState::Error, 300, "Invalid transition");
    EXPECT_EQ(resp.request_id, 456);
    EXPECT_FALSE(resp.success);
    EXPECT_EQ(resp.current_state, ComponentState::Error);
    EXPECT_EQ(resp.error_code, 300);
    EXPECT_EQ(resp.error_message, "Invalid transition");
}

}  // namespace DELILA
```

### テストディレクトリ構造

```
tests/unit/core/
├── test_component_state.cpp      # 新規作成
├── test_command.cpp              # 新規作成
├── test_command_response.cpp     # 新規作成
├── test_error_code.cpp           # 新規作成
└── test_minimal_event_data.cpp   # 既存（変更なし）
```

### CMakeLists.txt 更新

```cmake
# tests/unit/core/CMakeLists.txt
add_executable(delila_core_tests
    test_component_state.cpp
    test_command.cpp
    test_command_response.cpp
    test_error_code.cpp
    test_minimal_event_data.cpp
)

target_link_libraries(delila_core_tests
    delila_core
    GTest::gtest_main
)

add_test(NAME CoreTests COMMAND delila_core_tests)
```

---

## 完了条件

- [ ] lib/core ディレクトリ構造作成
- [ ] ComponentState.hpp 移動
- [ ] ComponentStatus.hpp 作成
- [ ] Command.hpp / CommandResponse.hpp 作成
- [ ] ErrorCode.hpp 作成
- [ ] CMakeLists.txt 作成
- [ ] lib/net から旧ファイル削除
- [ ] 既存テスト削除（test_component_state.cpp in net/）
- [ ] 新規テスト作成（tests/unit/core/）
- [ ] 全テストがパス
- [ ] ビルド確認

---

## 次のPhase

Phase 0-Core 完了後、Phase 0（lib/net 基盤整備）に進む。
