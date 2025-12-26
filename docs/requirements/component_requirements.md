# コンポーネント部品 要求仕様書

本ドキュメントは、DELILA2 のネットワークコンポーネント部品の要求仕様を定義します。

---

## 1. Command通信 (REQ/REP)

### 1.1 概要
- **方向**: Operator → Component (一方向)
- **パターン**: ZeroMQ REQ/REP
- **シリアライズ形式**: JSON
- **送信方式**: 個別送信（各コンポーネントに1対1で送信）

### 1.2 コマンド種類

| コマンド | 説明 | パラメータ | 期待される状態遷移 |
|---------|------|-----------|-------------------|
| CONFIGURE | 設定適用 | config: JSON | Loaded/Error → Configured |
| ARM | 取得準備 | なし | Configured → Armed |
| START | 取得開始 | なし | Armed → Running |
| STOP | 取得停止 | なし | Running/Armed → Loaded |
| STATUS | 状態取得 | なし | 変化なし |
| RESET | リセット | なし | Any → Loaded |
| CUSTOM | カスタム | type: string, params: JSON | コマンド依存 |

### 1.3 タイムアウト設定

| コマンド | デフォルトタイムアウト | 理由 |
|---------|---------------------|------|
| CONFIGURE | 30秒 | ハードウェア設定に時間がかかる場合あり |
| ARM | 10秒 | ハードウェア準備 |
| START | 5秒 | 即座に開始すべき |
| STOP | 10秒 | データフラッシュを待つ |
| STATUS | 2秒 | 即座に応答すべき |
| RESET | 10秒 | クリーンアップ処理 |
| CUSTOM | 設定可能 | コマンド依存 |

### 1.4 ビジー処理
- コンポーネントがビジー状態でも**割り込み可能**
- 現在の処理を中断して新コマンドを優先処理
- 中断された処理の結果は破棄

### 1.5 ターゲット指定
- **component_id**: 個別コンポーネントを指定
- **group_id**: グループ単位で指定（オプション）
- 両方を指定可能

### 1.6 Command構造体

```cpp
struct Command {
    std::string command_type;     // "CONFIGURE", "ARM", "START", etc.
    std::string target_id;        // コンポーネントID または グループID
    std::string group_id;         // オプション: グループID
    uint64_t request_id;          // リクエスト追跡用
    uint32_t timeout_ms;          // タイムアウト（ミリ秒）
    nlohmann::json params;        // コマンドパラメータ
    std::chrono::system_clock::time_point timestamp;
};
```

### 1.7 CommandResponse構造体

```cpp
struct CommandResponse {
    uint64_t request_id;          // 対応するリクエストID
    bool success;                 // 成功/失敗
    ComponentState current_state; // 現在の状態
    ErrorCode error_code;         // エラーコード（失敗時）
    std::string error_message;    // エラー詳細メッセージ
    std::chrono::system_clock::time_point timestamp;
};
```

---

## 2. Metrics収集

### 2.1 概要
- **送信頻度**: 定期送信（1秒ごと） + オンデマンド
- **送信チャネル**: Status チャネル (PUB/SUB)

### 2.2 収集項目

| カテゴリ | メトリクス名 | 型 | 説明 |
|---------|-------------|-----|------|
| スループット | events_per_second | double | 1秒あたりのイベント数 |
| スループット | bytes_per_second | double | 1秒あたりのバイト数 |
| バッファ | queue_size | uint64_t | 現在のキューサイズ |
| バッファ | queue_capacity | uint64_t | キューの最大容量 |
| バッファ | dropped_events | uint64_t | 欠落イベント数 |
| レイテンシ | avg_latency_us | double | 平均レイテンシ（マイクロ秒） |
| レイテンシ | max_latency_us | double | 最大レイテンシ（マイクロ秒） |
| エラー | error_count | uint64_t | エラー発生回数 |
| 稼働 | uptime_seconds | uint64_t | 稼働時間（秒） |
| シーケンス | sequence_gaps | uint64_t | シーケンスギャップ数 |

### 2.3 Metrics構造体

```cpp
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
};
```

### 2.4 MetricsCollector クラス

```cpp
class MetricsCollector {
public:
    void RecordEvent(size_t bytes);
    void RecordLatency(std::chrono::microseconds latency);
    void RecordError();
    void RecordSequenceGap();
    void SetQueueSize(uint64_t size, uint64_t capacity);

    Metrics GetCurrentMetrics() const;
    void Reset();

private:
    std::atomic<uint64_t> event_count_;
    std::atomic<uint64_t> byte_count_;
    // ... その他の内部状態
};
```

---

## 3. Error Handling

### 3.1 エラーコード体系

```cpp
enum class ErrorCategory : uint16_t {
    NONE = 0x0000,
    NETWORK = 0x1000,    // ネットワークエラー
    HARDWARE = 0x2000,   // ハードウェアエラー
    CONFIG = 0x3000,     // 設定エラー
    STATE = 0x4000,      // 状態遷移エラー
    DATA = 0x5000,       // データエラー
    TIMEOUT = 0x6000,    // タイムアウト
    INTERNAL = 0x7000,   // 内部エラー
};

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
```

### 3.2 エラー状態からのリカバリ

- **ERROR状態**: 異常が発生した際に遷移
- **リカバリ方法**: `CONFIGURE` コマンドでリセット可能
- **遷移**: ERROR → Configured (CONFIGURE成功時)

### 3.3 状態遷移図（ERROR状態含む）

```
                    ┌─────────────────────────────────────┐
                    │                                     │
                    ▼                                     │
    ┌────────┐  CONFIGURE  ┌────────────┐  ARM  ┌───────┐│  START  ┌─────────┐
    │ Loaded │────────────▶│ Configured │──────▶│ Armed ││────────▶│ Running │
    └────────┘             └────────────┘       └───────┘│         └─────────┘
        ▲                        ▲                  ▲    │              │
        │                        │                  │    │              │
        │                   CONFIGURE               │    │              │
        │                        │                  │    │              │
        │                   ┌────┴────┐             │    │              │
        │                   │  ERROR  │◀────────────┴────┴──────────────┘
        │                   └─────────┘        (エラー発生時)
        │
        └──────────────────────────────────────────────────────────────────
                                STOP
```

---

## 4. IComponent インターフェース

### 4.1 概要
- ComponentBase抽象クラスは**不要**
- `IComponent` インターフェースのみ定義
- 各コンポーネント (Source/Sink) が個別に実装

### 4.2 インターフェース定義

```cpp
class IComponent {
public:
    virtual ~IComponent() = default;

    // ライフサイクル
    virtual bool Initialize(const nlohmann::json& config) = 0;
    virtual bool Configure() = 0;
    virtual bool Arm() = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual bool Reset() = 0;

    // 状態
    virtual ComponentState GetState() const = 0;
    virtual std::string GetComponentId() const = 0;
    virtual std::string GetGroupId() const = 0;

    // メトリクス
    virtual Metrics GetMetrics() const = 0;

    // コマンド処理
    virtual CommandResponse HandleCommand(const Command& cmd) = 0;
};
```

---

## 5. Ready Signal

### 5.1 決定事項
- **Ready Signal は不要**
- コマンド応答に含まれる `ComponentState` で十分
- ARM完了はコマンド応答の `current_state == Armed` で確認

---

## 6. 実装タスクリスト

### Phase 1: Command通信基盤 (優先度: 最高)

| ID | タスク | 見積もり | 依存 |
|----|--------|---------|------|
| C-1 | Command構造体定義 | 0.5h | - |
| C-2 | CommandResponse構造体定義 | 0.5h | - |
| C-3 | ErrorCode enum定義 | 1h | - |
| C-4 | Command JSONシリアライズ/デシリアライズ | 2h | C-1, C-2 |
| C-5 | ZMQTransportにCommandSocket追加 | 2h | C-4 |
| C-6 | SendCommand/ReceiveCommand実装 | 2h | C-5 |
| C-7 | タイムアウト処理実装 | 1h | C-6 |
| C-8 | Command通信ユニットテスト | 3h | C-7 |
| C-9 | Command通信統合テスト | 2h | C-8 |

### Phase 2: Metrics収集 (優先度: 高)

| ID | タスク | 見積もり | 依存 |
|----|--------|---------|------|
| M-1 | Metrics構造体定義 | 0.5h | - |
| M-2 | MetricsCollectorクラス実装 | 3h | M-1 |
| M-3 | ComponentStatusにMetrics統合 | 1h | M-2 |
| M-4 | 定期送信機構実装 | 2h | M-3 |
| M-5 | Metricsユニットテスト | 2h | M-2 |
| M-6 | Metrics統合テスト | 2h | M-4, M-5 |

### Phase 3: Error Handling (優先度: 高)

| ID | タスク | 見積もり | 依存 |
|----|--------|---------|------|
| E-1 | ErrorCategory/ErrorCode定義 | 1h | - |
| E-2 | ComponentStateにERROR追加 | 0.5h | E-1 |
| E-3 | 状態遷移ロジック更新 (ERROR対応) | 2h | E-2 |
| E-4 | CONFIGUREでのERRORリカバリ実装 | 1h | E-3 |
| E-5 | エラーハンドリングユニットテスト | 2h | E-4 |

### Phase 4: IComponent インターフェース (優先度: 中)

| ID | タスク | 見積もり | 依存 |
|----|--------|---------|------|
| I-1 | IComponentインターフェース定義 | 1h | Phase 1-3 |
| I-2 | コマンドハンドラ基本実装 | 2h | I-1 |
| I-3 | IComponentテスト（モック使用） | 2h | I-2 |

### 合計見積もり
- Phase 1: 約14時間
- Phase 2: 約10.5時間
- Phase 3: 約6.5時間
- Phase 4: 約5時間
- **総計: 約36時間**

---

## 7. JSON フォーマット例

### 7.1 Command JSON

```json
{
    "command_type": "ARM",
    "target_id": "digitizer_01",
    "group_id": "sources",
    "request_id": 12345,
    "timeout_ms": 10000,
    "params": {},
    "timestamp": "2024-01-15T10:30:00.123Z"
}
```

### 7.2 CommandResponse JSON

```json
{
    "request_id": 12345,
    "success": true,
    "current_state": "Armed",
    "error_code": 0,
    "error_message": "",
    "timestamp": "2024-01-15T10:30:00.456Z"
}
```

### 7.3 Metrics JSON (ComponentStatus内)

```json
{
    "component_id": "digitizer_01",
    "state": "Running",
    "metrics": {
        "events_per_second": 125000.5,
        "bytes_per_second": 12500000.0,
        "queue_size": 1024,
        "queue_capacity": 10240,
        "dropped_events": 0,
        "avg_latency_us": 150.5,
        "max_latency_us": 2500.0,
        "error_count": 0,
        "uptime_seconds": 3600,
        "sequence_gaps": 0
    },
    "heartbeat_counter": 3600,
    "timestamp": "2024-01-15T10:30:00.789Z"
}
```

---

## 8. 設計原則

1. **KISS (Keep It Simple, Stupid)**: シンプルな設計を優先
2. **TDD (Test-Driven Development)**: テストを先に書く
3. **JSON**: 人間が読みやすく、デバッグしやすい形式を使用
4. **割り込み可能**: コマンドは常に受け付け可能
5. **状態明示**: 応答には常に現在状態を含める
