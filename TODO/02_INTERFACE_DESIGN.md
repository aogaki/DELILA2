# インターフェース設計

## 1. 状態定義

### ComponentState

```cpp
// lib/core/include/delila/core/ComponentState.hpp

namespace DELILA {

enum class ComponentState : uint8_t {
    Idle = 0,        // 初期状態、未設定
    Configuring = 1, // 設定処理中（過渡状態）
    Configured = 2,  // 設定完了
    Arming = 3,      // Arm処理中（過渡状態）
    Armed = 4,       // トリガー待ち
    Starting = 5,    // Start処理中（過渡状態）
    Running = 6,     // 実行中
    Stopping = 7,    // 停止処理中（過渡状態）
    Error = 8        // エラー
};

// 状態名を文字列に変換
std::string ComponentStateToString(ComponentState state);

// 状態遷移が有効か確認
bool IsValidTransition(ComponentState from, ComponentState to);

}  // namespace DELILA
```

### 状態遷移図

```
Idle ──[Configure]──▶ Configuring ──[成功]──▶ Configured
                           │                      │
                           └──[失敗]──▶ Error     │
                                                  │
Configured ──[Arm]──▶ Arming ──[成功]──▶ Armed   │
     ▲                   │                  │     │
     │                   └──[失敗]──▶ Error │     │
     │                                      │     │
     │        Armed ──[Start]──▶ Starting ──[成功]──▶ Running
     │                             │                    │
     │                             └──[失敗]──▶ Error   │
     │                                                  │
     └──────────────[Stop成功]──── Stopping ◀──[Stop]───┘
                                      │
                                      └──[失敗]──▶ Error

Error ──[Reset]──▶ Idle
Any State ──[致命的エラー]──▶ Error
```

### 遷移ルール

| 開始状態 | 許可される遷移先 |
|----------|------------------|
| Idle | Configuring |
| Configuring | Configured, Error |
| Configured | Arming |
| Arming | Armed, Error |
| Armed | Starting |
| Starting | Running, Error |
| Running | Stopping |
| Stopping | Configured, Error |
| Error | Idle（Resetのみ） |
| Any | Idle, Error |

---

## 2. IComponent（基底インターフェース）

全コンポーネントが実装する共通インターフェース。

```cpp
// lib/core/include/delila/core/IComponent.hpp

namespace DELILA {

class IComponent {
public:
    virtual ~IComponent() = default;

    // ライフサイクル（main/frameworkから呼ばれる）
    virtual bool Initialize(const std::string& config_path) = 0;
    virtual void Run() = 0;      // メインループ開始（ブロッキング）
    virtual void Shutdown() = 0; // 終了処理

    // 状態取得
    virtual ComponentState GetState() const = 0;
    virtual std::string GetComponentId() const = 0;
    virtual ComponentStatus GetStatus() const = 0;

protected:
    // 派生クラスが実装するコールバック
    virtual bool OnConfigure(const nlohmann::json& config) = 0;
    virtual bool OnArm() = 0;
    virtual bool OnStart(uint32_t run_number) = 0;
    virtual bool OnStop(bool graceful) = 0;
    virtual void OnReset() = 0;
};

}  // namespace DELILA
```

### メソッド説明

| メソッド | 呼び出し元 | 説明 |
|----------|------------|------|
| Initialize | main/framework | 設定ファイルを読み込み初期化 |
| Run | main/framework | メインループ開始（ブロッキング） |
| Shutdown | main/framework | 終了処理 |
| GetState | 任意 | 現在の状態を取得 |
| GetComponentId | 任意 | コンポーネントIDを取得 |
| GetStatus | 任意 | 詳細ステータスを取得 |
| OnConfigure | コマンドスレッド | Configureコマンド受信時 |
| OnArm | コマンドスレッド | Armコマンド受信時 |
| OnStart | コマンドスレッド | Startコマンド受信時 |
| OnStop | コマンドスレッド | Stopコマンド受信時 |
| OnReset | コマンドスレッド | Resetコマンド受信時 |

---

## 3. IDataComponent（データコンポーネント）

IComponentを継承。データの入出力を行うコンポーネント。

```cpp
// lib/core/include/delila/core/IDataComponent.hpp

namespace DELILA {

class IDataComponent : public IComponent {
public:
    virtual ~IDataComponent() = default;

    // 入出力アドレス設定
    virtual void SetInputAddresses(const std::vector<std::string>& addresses) = 0;
    virtual void SetOutputAddresses(const std::vector<std::string>& addresses) = 0;

    // アドレス取得
    virtual std::vector<std::string> GetInputAddresses() const = 0;
    virtual std::vector<std::string> GetOutputAddresses() const = 0;
};

}  // namespace DELILA
```

### 具体的なデータコンポーネント

| コンポーネント | 入力数 | 出力数 | 用途 |
|----------------|--------|--------|------|
| DigitizerSource | 0 | 1 | ハードウェアからデータ取得 |
| FileWriter | 1 | 0 | ファイルへデータ書き込み |
| OnlineAnalyzer | 1 | 0 | リアルタイム解析 |
| TimeSortMerger | N | 1 | 複数ソースを時系列ソート |

---

## 4. IOperator（オペレーター）

IComponentを継承。システム全体を制御するコンポーネント。

```cpp
// lib/core/include/delila/core/IOperator.hpp

namespace DELILA {

// 非同期ジョブの状態
enum class JobState {
    Pending,    // 待機中
    Running,    // 実行中
    Completed,  // 完了
    Failed      // 失敗
};

struct JobStatus {
    std::string job_id;
    JobState state;
    std::string error_message;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point completed_at;
};

class IOperator : public IComponent {
public:
    virtual ~IOperator() = default;

    // 非同期コマンド（job_idを返す）
    virtual std::string ConfigureAllAsync() = 0;
    virtual std::string ArmAllAsync() = 0;
    virtual std::string StartAllAsync(uint32_t run_number) = 0;
    virtual std::string StopAllAsync(bool graceful) = 0;
    virtual std::string ResetAllAsync() = 0;

    // ジョブ状態取得
    virtual JobStatus GetJobStatus(const std::string& job_id) const = 0;

    // ステータス取得
    virtual std::vector<ComponentStatus> GetAllComponentStatus() const = 0;
    virtual ComponentStatus GetComponentStatus(const std::string& component_id) const = 0;

    // コンポーネント管理
    virtual std::vector<std::string> GetComponentIds() const = 0;
    virtual bool IsAllInState(ComponentState state) const = 0;
};

}  // namespace DELILA
```

### 非同期処理の理由

- コマンド送信は複数コンポーネントに対して行われる
- 各コンポーネントの応答時間は不定
- UIスレッドをブロックしない
- job_idでポーリング可能

---

## 5. メッセージ構造

### Command

Operator → Component へのコマンド。

```cpp
// lib/core/include/delila/core/Command.hpp

namespace DELILA {

enum class CommandType : uint8_t {
    // ライフサイクルコマンド
    Configure = 0,
    Arm = 1,
    Start = 2,
    Stop = 3,
    Reset = 4,

    // クエリコマンド
    GetStatus = 10,
    GetConfig = 11,

    // ユーティリティコマンド
    Ping = 20
};

struct Command {
    CommandType type;        // コマンド種別
    uint32_t request_id;     // リクエストID（応答との紐付け）
    std::string config_path; // 設定ファイルパス（Configureの場合）
    uint32_t run_number;     // ラン番号（Startの場合）
    bool graceful;           // グレースフル停止（Stopの場合）
    std::string payload;     // 追加データ（JSON）
};

}  // namespace DELILA
```

### CommandResponse

Component → Operator への応答。

```cpp
// lib/core/include/delila/core/CommandResponse.hpp

namespace DELILA {

struct CommandResponse {
    uint32_t request_id;          // リクエストID
    bool success;                 // 成功/失敗
    ErrorCode error_code;         // エラーコード
    ComponentState current_state; // 現在の状態
    std::string message;          // メッセージ
    std::string payload;          // 追加データ（JSON）

    // ファクトリメソッド
    static CommandResponse Success(uint32_t id, ComponentState state,
                                   const std::string& msg = "");
    static CommandResponse Error(uint32_t id, ErrorCode code,
                                 const std::string& msg,
                                 ComponentState state = ComponentState::Error);
};

}  // namespace DELILA
```

### ComponentStatus

Component → Operator への定期ステータス報告。

```cpp
// lib/core/include/delila/core/ComponentStatus.hpp

namespace DELILA {

struct ComponentMetrics {
    uint64_t events_processed;   // 処理イベント数
    uint64_t bytes_transferred;  // 転送バイト数
    uint32_t queue_size;         // 現在のキューサイズ
    uint32_t queue_max;          // キュー最大サイズ
    double event_rate;           // イベントレート (events/s)
    double data_rate;            // データレート (MB/s)
};

struct ComponentStatus {
    std::string component_id;    // コンポーネントID
    ComponentState state;        // 現在の状態
    uint64_t timestamp;          // Unix timestamp (ms)
    uint32_t run_number;         // 現在のラン番号
    ComponentMetrics metrics;    // パフォーマンスメトリクス
    std::string error_message;   // エラー時のメッセージ
    uint64_t heartbeat_counter;  // ハートビートカウンタ
};

}  // namespace DELILA
```

---

## 6. エラーコード分類

エラーコードはカテゴリごとに範囲を分ける。

```cpp
// lib/core/include/delila/core/ErrorCode.hpp

namespace DELILA {

enum class ErrorCode {
    Success = 0,

    // 設定エラー (100-199)
    // 設定ファイルが見つからない、パースエラー、バリデーションエラーなど

    // 状態エラー (200-299)
    // 無効な状態遷移、未設定、未Arm、既に実行中など

    // ハードウェアエラー (300-399)
    // デバイスが見つからない、初期化失敗、通信エラーなど

    // 通信エラー (400-499)
    // 接続失敗、タイムアウト、送受信エラーなど

    // 内部エラー (500-599)
    // 内部エラー、メモリ不足、スレッドエラーなど

    Unknown = 999
};

}  // namespace DELILA
```

---

## 7. 設定構造

### ComponentConfig

個々のコンポーネント用設定。

```cpp
// lib/core/include/delila/core/ComponentConfig.hpp

namespace DELILA {

struct TransportConfig {
    std::string data_address;     // データチャネルアドレス
    std::string status_address;   // ステータスチャネルアドレス
    std::string command_address;  // コマンドチャネルアドレス
    bool bind_data;               // データチャネルをbindするか
    bool bind_status;             // ステータスチャネルをbindするか
    bool bind_command;            // コマンドチャネルをbindするか
    std::string data_pattern;     // データパターン（PUSH/PULL, PUB/SUB）
};

struct ComponentConfig {
    std::string component_id;     // コンポーネントID
    std::string component_type;   // コンポーネント種別

    // 入出力
    std::vector<std::string> input_addresses;
    std::vector<std::string> output_addresses;

    // ネットワーク
    TransportConfig transport;

    // バッファ
    uint32_t queue_max_size;
    uint32_t queue_warning_threshold;

    // タイミング
    uint32_t status_interval_ms;
    uint32_t command_timeout_ms;
};

}  // namespace DELILA
```

### OperatorConfig

オペレーター用設定。

```cpp
// lib/core/include/delila/core/OperatorConfig.hpp

namespace DELILA {

struct ComponentAddress {
    std::string component_id;     // コンポーネントID
    std::string command_address;  // コマンドアドレス
    std::string status_address;   // ステータスアドレス
    std::string component_type;   // コンポーネント種別
    int start_order;              // 開始順序（小さい方が先）
};

struct OperatorConfig {
    std::string operator_id;
    std::vector<ComponentAddress> components;

    // タイムアウト
    uint32_t configure_timeout_ms;
    uint32_t arm_timeout_ms;
    uint32_t start_timeout_ms;
    uint32_t stop_timeout_ms;

    // リトライ
    uint32_t command_retry_count;
    uint32_t command_retry_interval_ms;
};

}  // namespace DELILA
```

---

## 8. ファイル配置

```
lib/core/
├── CMakeLists.txt
└── include/delila/core/
    ├── ComponentState.hpp
    ├── IComponent.hpp
    ├── IDataComponent.hpp
    ├── IOperator.hpp
    ├── ComponentStatus.hpp
    ├── Command.hpp
    ├── CommandResponse.hpp
    ├── ErrorCode.hpp
    ├── ComponentConfig.hpp
    └── OperatorConfig.hpp

lib/component/
├── CMakeLists.txt
├── include/
│   ├── DigitizerSource.hpp
│   ├── FileWriter.hpp
│   ├── TimeSortMerger.hpp
│   └── CLIOperator.hpp
└── src/
    ├── DigitizerSource.cpp
    ├── FileWriter.cpp
    ├── TimeSortMerger.cpp
    └── CLIOperator.cpp
```

---

## 9. 関連ドキュメント

- [00_OVERVIEW.md](00_OVERVIEW.md) - 全体概要
- [01_REQUIREMENTS.md](01_REQUIREMENTS.md) - 要求仕様
- [03_IMPLEMENTATION_PLAN.md](03_IMPLEMENTATION_PLAN.md) - 実装計画
- [04_TEST_STRATEGY.md](04_TEST_STRATEGY.md) - テスト戦略
