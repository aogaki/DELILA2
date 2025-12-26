# Phase 0: 基盤整備

**見積もり**: 3.5時間
**目的**: ディレクトリ構造の作成とインターフェース定義

---

## タスク一覧

### P0-1: ディレクトリ構造作成
**見積もり**: 30分

```bash
lib/net/include/
├── interfaces/
├── domain/
├── application/
├── infrastructure/
│   ├── zmq/
│   ├── serialization/
│   └── checksum/
├── utils/
└── facade/

lib/net/src/
├── domain/
├── application/
├── infrastructure/
│   ├── zmq/
│   ├── serialization/
│   └── checksum/
└── facade/
```

**完了条件**:
- [ ] 全ディレクトリ作成
- [ ] 各ディレクトリに.gitkeepまたはREADME.md

---

### P0-2: IDataChannel インターフェース定義
**見積もり**: 30分

```cpp
// lib/net/include/interfaces/IDataChannel.hpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace DELILA::Net {

/**
 * @brief データ送受信チャネルのインターフェース
 *
 * バイナリデータの送受信を抽象化。
 * 実装: ZMQDataChannel (PUB/SUB, PUSH/PULL)
 */
class IDataChannel {
public:
    virtual ~IDataChannel() = default;

    /**
     * @brief チャネルに接続
     * @return 成功時 true
     */
    virtual bool Connect() = 0;

    /**
     * @brief チャネルを切断
     */
    virtual void Disconnect() = 0;

    /**
     * @brief 接続状態を確認
     */
    virtual bool IsConnected() const = 0;

    /**
     * @brief バイナリデータを送信
     * @param data 送信データ
     * @return 成功時 true
     */
    virtual bool Send(std::span<const uint8_t> data) = 0;

    /**
     * @brief バイナリデータを受信
     * @return 受信データ（タイムアウト時は空のoptional）
     */
    virtual std::optional<std::vector<uint8_t>> Receive() = 0;

    /**
     * @brief 受信タイムアウトを設定
     * @param timeout_ms タイムアウト（ミリ秒）
     */
    virtual void SetReceiveTimeout(int timeout_ms) = 0;
};

}  // namespace DELILA::Net
```

**テスト項目**:
- [ ] インターフェースがコンパイル可能
- [ ] 純粋仮想関数のみ

---

### P0-3: IStatusChannel インターフェース定義
**見積もり**: 30分

```cpp
// lib/net/include/interfaces/IStatusChannel.hpp
#pragma once

#include <memory>
#include <optional>

// Forward declaration (ドメイン層)
namespace DELILA::Net {
struct ComponentStatus;
}

namespace DELILA::Net {

/**
 * @brief ステータス送受信チャネルのインターフェース
 *
 * コンポーネントステータスの送受信を抽象化。
 * 実装: ZMQStatusChannel (PUB/SUB)
 */
class IStatusChannel {
public:
    virtual ~IStatusChannel() = default;

    virtual bool Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;

    /**
     * @brief ステータスを送信
     */
    virtual bool SendStatus(const ComponentStatus& status) = 0;

    /**
     * @brief ステータスを受信
     */
    virtual std::optional<ComponentStatus> ReceiveStatus() = 0;
};

}  // namespace DELILA::Net
```

---

### P0-4: ICommandChannel インターフェース定義
**見積もり**: 30分

```cpp
// lib/net/include/interfaces/ICommandChannel.hpp
#pragma once

#include <chrono>
#include <memory>
#include <optional>

// Forward declarations (ドメイン層)
namespace DELILA::Net {
struct Command;
struct CommandResponse;
}

namespace DELILA::Net {

/**
 * @brief コマンド送受信チャネルのインターフェース
 *
 * REQ/REP パターンでコマンドを送受信。
 * - Operator側: SendCommand → ReceiveResponse
 * - Component側: ReceiveCommand → SendResponse
 */
class ICommandChannel {
public:
    virtual ~ICommandChannel() = default;

    virtual bool Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;

    // --- Operator側 (REQ) ---

    /**
     * @brief コマンドを送信し、レスポンスを待つ
     * @param command 送信するコマンド
     * @param timeout タイムアウト
     * @return レスポンス（タイムアウト時は空のoptional）
     */
    virtual std::optional<CommandResponse> SendCommand(
        const Command& command,
        std::chrono::milliseconds timeout) = 0;

    // --- Component側 (REP) ---

    /**
     * @brief コマンドを受信（ブロッキング）
     * @param timeout タイムアウト
     * @return 受信したコマンド
     */
    virtual std::optional<Command> ReceiveCommand(
        std::chrono::milliseconds timeout) = 0;

    /**
     * @brief レスポンスを送信
     */
    virtual bool SendResponse(const CommandResponse& response) = 0;
};

}  // namespace DELILA::Net
```

---

### P0-5: ISerializer インターフェース定義
**見積もり**: 30分

```cpp
// lib/net/include/interfaces/ISerializer.hpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

// Forward declarations
namespace DELILA::Digitizer {
class EventData;
class MinimalEventData;
}

namespace DELILA::Net {

/**
 * @brief シリアライズのインターフェース
 *
 * EventData/MinimalEventData のバイナリ変換を抽象化。
 */
template<typename T>
class ISerializer {
public:
    virtual ~ISerializer() = default;

    /**
     * @brief オブジェクトをバイナリにシリアライズ
     */
    virtual std::vector<uint8_t> Serialize(
        const std::vector<std::unique_ptr<T>>& objects) = 0;

    /**
     * @brief バイナリからオブジェクトにデシリアライズ
     */
    virtual std::optional<std::vector<std::unique_ptr<T>>> Deserialize(
        std::span<const uint8_t> data) = 0;
};

// 型エイリアス
using IEventDataSerializer = ISerializer<DELILA::Digitizer::EventData>;
using IMinimalEventDataSerializer = ISerializer<DELILA::Digitizer::MinimalEventData>;

}  // namespace DELILA::Net
```

---

### P0-6: IChecksum インターフェース定義
**見積もり**: 30分

```cpp
// lib/net/include/interfaces/IChecksum.hpp
#pragma once

#include <cstdint>
#include <span>

namespace DELILA::Net {

/**
 * @brief チェックサム計算のインターフェース
 *
 * 実装: CRC32Checksum
 */
class IChecksum {
public:
    virtual ~IChecksum() = default;

    /**
     * @brief チェックサムを計算
     */
    virtual uint32_t Calculate(std::span<const uint8_t> data) = 0;

    /**
     * @brief チェックサムを検証
     */
    virtual bool Verify(std::span<const uint8_t> data, uint32_t expected) = 0;

    /**
     * @brief チェックサムタイプ識別子
     */
    virtual uint8_t GetTypeId() const = 0;
};

}  // namespace DELILA::Net
```

---

### P0-7: CMakeLists.txt 更新
**見積もり**: 30分

```cmake
# lib/net/CMakeLists.txt

# ソースファイルの収集
file(GLOB_RECURSE NET_SOURCES
    "src/domain/*.cpp"
    "src/application/*.cpp"
    "src/infrastructure/*.cpp"
    "src/facade/*.cpp"
)

# ヘッダーファイルのインクルードパス
target_include_directories(delila_net PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/interfaces>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/domain>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/application>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/infrastructure>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/utils>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/facade>
)
```

---

---

## テスト戦略

### 既存テストの削除

このフェーズでは**削除するテストはない**。
インターフェース定義のみのため、テストは不要。

### 新規テスト作成

インターフェースは純粋仮想関数のみで構成されるため、直接のテストは不要。
ただし、インターフェースがコンパイル可能であることを確認するための最小限のテストを作成する。

**ファイル**: `tests/unit/net/interfaces/test_interface_compilation.cpp`

```cpp
// tests/unit/net/interfaces/test_interface_compilation.cpp
// インターフェースがコンパイル可能であることを確認

#include <gtest/gtest.h>
#include "interfaces/IDataChannel.hpp"
#include "interfaces/IStatusChannel.hpp"
#include "interfaces/ICommandChannel.hpp"
#include "interfaces/ISerializer.hpp"
#include "interfaces/IChecksum.hpp"

namespace DELILA::Net {

// モック実装（コンパイル確認用）
class MockDataChannel : public IDataChannel {
public:
    bool Connect() override { return true; }
    void Disconnect() override {}
    bool IsConnected() const override { return true; }
    bool Send(std::span<const uint8_t>) override { return true; }
    std::optional<std::vector<uint8_t>> Receive() override { return std::nullopt; }
    void SetReceiveTimeout(int) override {}
};

class MockChecksum : public IChecksum {
public:
    uint32_t Calculate(std::span<const uint8_t>) override { return 0; }
    bool Verify(std::span<const uint8_t>, uint32_t) override { return true; }
    uint8_t GetTypeId() const override { return 0; }
};

// コンパイルテスト
TEST(InterfaceCompilationTest, DataChannelCompiles) {
    MockDataChannel channel;
    EXPECT_TRUE(channel.Connect());
}

TEST(InterfaceCompilationTest, ChecksumCompiles) {
    MockChecksum checksum;
    EXPECT_EQ(checksum.GetTypeId(), 0);
}

}  // namespace DELILA::Net
```

### テストディレクトリ構造

```
tests/unit/net/
├── interfaces/                           # 新規ディレクトリ
│   └── test_interface_compilation.cpp    # 新規作成
└── ... (既存ファイル)
```

---

## 完了条件

- [ ] 全ディレクトリ構造が作成されている
- [ ] 全インターフェースがコンパイル可能
- [ ] CMakeLists.txt が更新され、ビルドが通る
- [ ] インターフェースコンパイルテストがパス

---

## 次のPhase

Phase 0 完了後、Phase 1（ドメイン層）に進む。
