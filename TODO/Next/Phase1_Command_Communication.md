# Phase 1: Command通信基盤

**優先度**: 最高
**目的**: OperatorからComponentへのコマンド送受信機能を実装
**パターン**: ZeroMQ REQ/REP

---

## タスク一覧

### C-1: Command構造体定義
**ファイル**: `lib/net/include/Command.hpp`
**見積もり**: 30分
**依存**: なし

#### 実装内容
```cpp
// lib/net/include/Command.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace DELILA::Net {

/**
 * @brief コマンドタイプの列挙型
 */
enum class CommandType {
    CONFIGURE,  // 設定適用
    ARM,        // 取得準備（Two-Phase Start Phase 1）
    START,      // 取得開始（Two-Phase Start Phase 2）
    STOP,       // 取得停止
    STATUS,     // 状態取得
    RESET,      // リセット
    CUSTOM      // カスタムコマンド
};

/**
 * @brief Operatorからコンポーネントへ送信するコマンド
 */
struct Command {
    CommandType command_type;                              // コマンド種類
    std::string command_type_str;                          // カスタムコマンド用文字列
    std::string target_id;                                 // 対象コンポーネントID
    std::string group_id;                                  // 対象グループID（オプション）
    uint64_t request_id = 0;                               // リクエスト追跡用ID
    uint32_t timeout_ms = 5000;                            // タイムアウト（ミリ秒）
    nlohmann::json params;                                 // コマンドパラメータ
    std::chrono::system_clock::time_point timestamp;       // 送信タイムスタンプ

    // デフォルトタイムアウト値
    static constexpr uint32_t DEFAULT_CONFIGURE_TIMEOUT_MS = 30000;
    static constexpr uint32_t DEFAULT_ARM_TIMEOUT_MS = 10000;
    static constexpr uint32_t DEFAULT_START_TIMEOUT_MS = 5000;
    static constexpr uint32_t DEFAULT_STOP_TIMEOUT_MS = 10000;
    static constexpr uint32_t DEFAULT_STATUS_TIMEOUT_MS = 2000;
    static constexpr uint32_t DEFAULT_RESET_TIMEOUT_MS = 10000;
};

// CommandType <-> 文字列変換
std::string CommandTypeToString(CommandType type);
CommandType StringToCommandType(const std::string& str);

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] CommandType列挙型の全値が正しく定義されている
- [ ] CommandTypeToString()が正しく変換する
- [ ] StringToCommandType()が正しく変換する
- [ ] 不正な文字列でCUSTOMが返される
- [ ] デフォルトタイムアウト値が正しい

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] 変換関数の実装ファイル作成
- [ ] ユニットテスト作成・パス

---

### C-2: CommandResponse構造体定義
**ファイル**: `lib/net/include/Command.hpp` (C-1と同じファイル)
**見積もり**: 30分
**依存**: C-1, E-1 (ErrorCode)

#### 実装内容
```cpp
// Command.hpp に追加

#include "ErrorCode.hpp"
#include "ComponentState.hpp"

/**
 * @brief コマンドに対するレスポンス
 */
struct CommandResponse {
    uint64_t request_id = 0;                               // 対応するリクエストID
    bool success = false;                                  // 成功/失敗
    ComponentState current_state = ComponentState::Loaded; // 現在の状態
    ErrorCode error_code = ErrorCode::SUCCESS;             // エラーコード
    std::string error_message;                             // エラー詳細メッセージ
    std::chrono::system_clock::time_point timestamp;       // 応答タイムスタンプ
};
```

#### テスト項目
- [ ] デフォルト値が正しく設定される
- [ ] 成功レスポンスを正しく作成できる
- [ ] 失敗レスポンスを正しく作成できる

#### 完了条件
- [ ] 構造体定義追加
- [ ] ユニットテスト作成・パス

---

### C-3: ErrorCode enum定義
**ファイル**: `lib/net/include/ErrorCode.hpp`
**見積もり**: 1時間
**依存**: なし

#### 実装内容
```cpp
// lib/net/include/ErrorCode.hpp
#pragma once

#include <cstdint>
#include <string>

namespace DELILA::Net {

/**
 * @brief エラーカテゴリ（上位バイト）
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
 * @brief エラーコード（カテゴリ + 詳細コード）
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

// ヘルパー関数
ErrorCategory GetErrorCategory(ErrorCode code);
std::string ErrorCodeToString(ErrorCode code);
std::string ErrorCategoryToString(ErrorCategory category);
bool IsSuccess(ErrorCode code);
bool IsNetworkError(ErrorCode code);
bool IsHardwareError(ErrorCode code);
bool IsStateError(ErrorCode code);

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] GetErrorCategory()が正しいカテゴリを返す
- [ ] ErrorCodeToString()が全コードで文字列を返す
- [ ] IsSuccess()がSUCCESSでのみtrueを返す
- [ ] IsNetworkError()がネットワークエラーでtrueを返す
- [ ] IsHardwareError()がハードウェアエラーでtrueを返す
- [ ] IsStateError()が状態エラーでtrueを返す

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] 実装ファイル作成 (`lib/net/src/ErrorCode.cpp`)
- [ ] ユニットテスト作成・パス (`tests/unit/net/test_error_code.cpp`)

---

### C-4: Command JSONシリアライズ/デシリアライズ
**ファイル**: `lib/net/src/Command.cpp`
**見積もり**: 2時間
**依存**: C-1, C-2, C-3

#### 実装内容
```cpp
// lib/net/src/Command.cpp

namespace DELILA::Net {

/**
 * @brief CommandをJSON文字列にシリアライズ
 */
std::string SerializeCommand(const Command& cmd);

/**
 * @brief JSON文字列からCommandにデシリアライズ
 */
Command DeserializeCommand(const std::string& json);

/**
 * @brief CommandResponseをJSON文字列にシリアライズ
 */
std::string SerializeCommandResponse(const CommandResponse& response);

/**
 * @brief JSON文字列からCommandResponseにデシリアライズ
 */
CommandResponse DeserializeCommandResponse(const std::string& json);

}  // namespace DELILA::Net
```

#### JSON形式
```json
// Command
{
    "command_type": "ARM",
    "target_id": "digitizer_01",
    "group_id": "sources",
    "request_id": 12345,
    "timeout_ms": 10000,
    "params": {},
    "timestamp": "2024-01-15T10:30:00.123Z"
}

// CommandResponse
{
    "request_id": 12345,
    "success": true,
    "current_state": "Armed",
    "error_code": 0,
    "error_message": "",
    "timestamp": "2024-01-15T10:30:00.456Z"
}
```

#### テスト項目
- [ ] Commandの全フィールドがシリアライズされる
- [ ] Commandの全フィールドがデシリアライズされる
- [ ] CommandResponseの全フィールドがシリアライズされる
- [ ] CommandResponseの全フィールドがデシリアライズされる
- [ ] 不正なJSONでエラーが発生する
- [ ] 必須フィールド欠落でエラーが発生する
- [ ] タイムスタンプがISO8601形式で正しく変換される
- [ ] ラウンドトリップ（シリアライズ→デシリアライズ）で元データと一致

#### 完了条件
- [ ] 実装ファイル作成
- [ ] ユニットテスト作成・パス (`tests/unit/net/test_command_serialization.cpp`)

---

### C-5: ZMQTransportにCommandSocket追加
**ファイル**: `lib/net/include/ZMQTransport.hpp`, `lib/net/src/ZMQTransport.cpp`
**見積もり**: 2時間
**依存**: C-4

#### 実装内容

##### ヘッダー変更
```cpp
// ZMQTransport.hpp に追加

struct TransportConfig {
    // ... 既存フィールド ...

    // コマンドチャネル設定
    std::string command_address = "tcp://localhost:5557";
    bool bind_command = false;  // サーバー側はtrue、クライアント側はfalse
    bool enable_command = true; // コマンドチャネルを有効にするか
};

class ZMQTransport {
public:
    // ... 既存メソッド ...

    // コマンド関連メソッド
    bool SendCommand(const Command& cmd);
    std::unique_ptr<Command> ReceiveCommand(int timeout_ms = -1);
    bool SendCommandResponse(const CommandResponse& response);
    std::unique_ptr<CommandResponse> ReceiveCommandResponse(int timeout_ms = -1);

private:
    // ... 既存フィールド ...

    std::unique_ptr<zmq::socket_t> fCommandSocket;  // REQ/REPソケット
};
```

##### 実装変更
```cpp
// ZMQTransport.cpp

bool ZMQTransport::Connect() {
    // ... 既存のデータソケット作成 ...

    // コマンドソケット作成
    if (fConfig.enable_command) {
        if (fConfig.bind_command) {
            // サーバー側: REPソケット
            fCommandSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_REP);
            fCommandSocket->bind(fConfig.command_address);
        } else {
            // クライアント側: REQソケット
            fCommandSocket = std::make_unique<zmq::socket_t>(*fContext, ZMQ_REQ);
            fCommandSocket->connect(fConfig.command_address);
        }
        fCommandSocket->set(zmq::sockopt::rcvtimeo, 5000);
    }

    // ...
}
```

#### テスト項目
- [ ] enable_command=falseでコマンドソケットが作成されない
- [ ] bind_command=trueでREPソケットがbindされる
- [ ] bind_command=falseでREQソケットがconnectされる
- [ ] コマンドアドレスが正しく使用される
- [ ] Disconnect()でコマンドソケットが正しくクローズされる

#### 完了条件
- [ ] ヘッダーファイル更新
- [ ] 実装ファイル更新
- [ ] ユニットテスト作成・パス

---

### C-6: SendCommand/ReceiveCommand実装
**ファイル**: `lib/net/src/ZMQTransport.cpp`
**見積もり**: 2時間
**依存**: C-5

#### 実装内容
```cpp
bool ZMQTransport::SendCommand(const Command& cmd) {
    if (!fConnected || !fCommandSocket) {
        return false;
    }

    try {
        std::string json = SerializeCommand(cmd);
        zmq::message_t message(json.size());
        std::memcpy(message.data(), json.c_str(), json.size());

        auto result = fCommandSocket->send(message, zmq::send_flags::none);
        return result.has_value();
    } catch (const zmq::error_t& e) {
        return false;
    }
}

std::unique_ptr<Command> ZMQTransport::ReceiveCommand(int timeout_ms) {
    if (!fConnected || !fCommandSocket) {
        return nullptr;
    }

    try {
        if (timeout_ms >= 0) {
            fCommandSocket->set(zmq::sockopt::rcvtimeo, timeout_ms);
        }

        zmq::message_t message;
        auto result = fCommandSocket->recv(message, zmq::recv_flags::none);

        if (!result.has_value() || message.size() == 0) {
            return nullptr;
        }

        std::string json(static_cast<const char*>(message.data()), message.size());
        return std::make_unique<Command>(DeserializeCommand(json));
    } catch (...) {
        return nullptr;
    }
}

bool ZMQTransport::SendCommandResponse(const CommandResponse& response) {
    // SendCommandと同様の実装
}

std::unique_ptr<CommandResponse> ZMQTransport::ReceiveCommandResponse(int timeout_ms) {
    // ReceiveCommandと同様の実装
}
```

#### テスト項目
- [ ] 接続前にSendCommandがfalseを返す
- [ ] 接続前にReceiveCommandがnullptrを返す
- [ ] 有効なCommandが正しく送信される
- [ ] 有効なCommandが正しく受信される
- [ ] 有効なCommandResponseが正しく送信される
- [ ] 有効なCommandResponseが正しく受信される
- [ ] REQ/REPの順序が守られる（送信→受信→送信→受信...）

#### 完了条件
- [ ] 4つのメソッド実装
- [ ] ユニットテスト作成・パス

---

### C-7: タイムアウト処理実装
**ファイル**: `lib/net/src/ZMQTransport.cpp`
**見積もり**: 1時間
**依存**: C-6

#### 実装内容
```cpp
std::unique_ptr<CommandResponse> ZMQTransport::SendCommandAndWait(
    const Command& cmd,
    uint32_t timeout_ms)
{
    if (!SendCommand(cmd)) {
        // 送信失敗時のエラーレスポンスを作成
        auto response = std::make_unique<CommandResponse>();
        response->request_id = cmd.request_id;
        response->success = false;
        response->error_code = ErrorCode::NETWORK_SEND_FAILED;
        response->error_message = "Failed to send command";
        return response;
    }

    auto response = ReceiveCommandResponse(timeout_ms);

    if (!response) {
        // タイムアウト時のエラーレスポンスを作成
        auto timeout_response = std::make_unique<CommandResponse>();
        timeout_response->request_id = cmd.request_id;
        timeout_response->success = false;
        timeout_response->error_code = ErrorCode::TIMEOUT_RESPONSE;
        timeout_response->error_message = "Command response timeout";
        return timeout_response;
    }

    return response;
}
```

#### テスト項目
- [ ] タイムアウト時にTIMEOUT_RESPONSEエラーが返る
- [ ] 送信失敗時にNETWORK_SEND_FAILEDエラーが返る
- [ ] 成功時に正しいレスポンスが返る
- [ ] request_idが正しく引き継がれる
- [ ] コマンド別デフォルトタイムアウトが適用される

#### 完了条件
- [ ] SendCommandAndWait()実装
- [ ] ユニットテスト作成・パス

---

### C-8: Command通信ユニットテスト
**ファイル**: `tests/unit/net/test_command.cpp`
**見積もり**: 3時間
**依存**: C-7

#### テストケース一覧

```cpp
// test_command.cpp

// === Command構造体テスト ===
TEST(CommandTest, DefaultValues)
TEST(CommandTest, CommandTypeToStringAllTypes)
TEST(CommandTest, StringToCommandTypeValid)
TEST(CommandTest, StringToCommandTypeInvalid)
TEST(CommandTest, DefaultTimeoutValues)

// === CommandResponse構造体テスト ===
TEST(CommandResponseTest, DefaultValues)
TEST(CommandResponseTest, SuccessResponse)
TEST(CommandResponseTest, FailureResponse)

// === ErrorCodeテスト ===
TEST(ErrorCodeTest, GetErrorCategoryNetwork)
TEST(ErrorCodeTest, GetErrorCategoryHardware)
TEST(ErrorCodeTest, GetErrorCategoryConfig)
TEST(ErrorCodeTest, GetErrorCategoryState)
TEST(ErrorCodeTest, ErrorCodeToStringAllCodes)
TEST(ErrorCodeTest, IsSuccessTrue)
TEST(ErrorCodeTest, IsSuccessFalse)
TEST(ErrorCodeTest, IsNetworkErrorTrue)
TEST(ErrorCodeTest, IsNetworkErrorFalse)

// === シリアライズテスト ===
TEST(CommandSerializationTest, SerializeBasicCommand)
TEST(CommandSerializationTest, SerializeCommandWithParams)
TEST(CommandSerializationTest, DeserializeValidCommand)
TEST(CommandSerializationTest, DeserializeInvalidJson)
TEST(CommandSerializationTest, DeserializeMissingFields)
TEST(CommandSerializationTest, RoundTrip)

TEST(CommandResponseSerializationTest, SerializeSuccessResponse)
TEST(CommandResponseSerializationTest, SerializeFailureResponse)
TEST(CommandResponseSerializationTest, DeserializeValid)
TEST(CommandResponseSerializationTest, RoundTrip)

// === ZMQTransport Commandテスト ===
TEST(ZMQTransportCommandTest, CommandSocketNotCreatedWhenDisabled)
TEST(ZMQTransportCommandTest, REPSocketBindsOnServer)
TEST(ZMQTransportCommandTest, REQSocketConnectsOnClient)
TEST(ZMQTransportCommandTest, SendCommandBeforeConnectFails)
TEST(ZMQTransportCommandTest, ReceiveCommandBeforeConnectReturnsNull)
```

#### 完了条件
- [ ] 全テストケース実装
- [ ] 全テストパス
- [ ] カバレッジ80%以上

---

### C-9: Command通信統合テスト
**ファイル**: `tests/integration/test_command_integration.cpp`
**見積もり**: 2時間
**依存**: C-8

#### テストケース一覧

```cpp
// test_command_integration.cpp

class CommandIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // サーバー側トランスポート設定
        serverConfig_.command_address = "tcp://127.0.0.1:15557";
        serverConfig_.bind_command = true;
        serverConfig_.enable_command = true;

        // クライアント側トランスポート設定
        clientConfig_.command_address = "tcp://127.0.0.1:15557";
        clientConfig_.bind_command = false;
        clientConfig_.enable_command = true;
    }

    TransportConfig serverConfig_;
    TransportConfig clientConfig_;
};

// サーバー・クライアント間の通信テスト
TEST_F(CommandIntegrationTest, SendReceiveCommand)
TEST_F(CommandIntegrationTest, SendReceiveResponse)
TEST_F(CommandIntegrationTest, FullRequestResponseCycle)
TEST_F(CommandIntegrationTest, MultipleCommandsCycle)
TEST_F(CommandIntegrationTest, TimeoutHandling)
TEST_F(CommandIntegrationTest, AllCommandTypes)
TEST_F(CommandIntegrationTest, CommandWithParams)
TEST_F(CommandIntegrationTest, ErrorResponseHandling)
```

#### 完了条件
- [ ] 全テストケース実装
- [ ] 全テストパス
- [ ] 実際のZMQソケット間通信で動作確認

---

## チェックリスト

### Phase 1 完了条件

- [ ] C-1: Command構造体定義 完了
- [ ] C-2: CommandResponse構造体定義 完了
- [ ] C-3: ErrorCode enum定義 完了
- [ ] C-4: Command JSONシリアライズ/デシリアライズ 完了
- [ ] C-5: ZMQTransportにCommandSocket追加 完了
- [ ] C-6: SendCommand/ReceiveCommand実装 完了
- [ ] C-7: タイムアウト処理実装 完了
- [ ] C-8: Command通信ユニットテスト 完了
- [ ] C-9: Command通信統合テスト 完了
- [ ] 全テスト合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

---

## 実装順序

```
C-1 ──┐
      ├──→ C-4 ──→ C-5 ──→ C-6 ──→ C-7 ──→ C-8 ──→ C-9
C-2 ──┤
      │
C-3 ──┘
```

**注意**: C-1, C-2, C-3は並行して実装可能
