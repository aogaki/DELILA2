# 要求仕様

## 1. インターフェース階層

```
IComponent (状態管理、コマンド、ステータス)
    ├── IDataComponent (データ入出力)
    └── IOperator (システム制御)
```

### 1.1 IComponent（基底インターフェース）

全コンポーネントが実装する共通インターフェース。

#### 必須メソッド

| メソッド | 説明 |
|----------|------|
| `Initialize(config_path)` | 設定ファイル（JSON/MongoDB）から初期化 |
| `GetState()` | 現在の状態を取得 |
| `GetComponentId()` | コンポーネントIDを取得 |

#### 状態とコマンドはスレッドで管理

- コマンド受信スレッドが状態遷移を制御
- メインスレッドは状態に応じた処理を実行

---

### 1.2 IDataComponent（データコンポーネント）

IComponentを継承。データの入出力を行うコンポーネント。

#### 追加メソッド

| メソッド | 説明 |
|----------|------|
| `SetInputAddresses(addresses)` | 入力元アドレスを設定（空なら入力なし） |
| `SetOutputAddresses(addresses)` | 出力先アドレスを設定（空なら出力なし） |

#### 具体例

| コンポーネント | InputAddresses | OutputAddresses |
|----------------|----------------|-----------------|
| DigitizerSource | 空 | 1つ |
| FileWriter | 1つ | 空 |
| OnlineAnalyzer | 1つ | 空 |
| TimeSortMerger | 複数 | 1つ |

#### 構成（Composition）

```cpp
class DigitizerSource : public IDataComponent {
private:
    std::unique_ptr<IDigitizer> fDigitizer;      // データ取得
    std::unique_ptr<ZMQTransport> fTransport;    // ネットワーク通信
    std::unique_ptr<DataProcessor> fProcessor;   // シリアライズ/デシリアライズ
};
```

---

### 1.3 IOperator（オペレーター）

IComponentを継承。システム全体を制御するコンポーネント。

#### 追加メソッド

| メソッド | 説明 |
|----------|------|
| `ConfigureAllAsync()` | 全コンポーネントをConfigure（非同期） |
| `ArmAllAsync()` | 全コンポーネントをArm（非同期） |
| `StartAllAsync()` | 全コンポーネントをStart（非同期） |
| `StopAllAsync(graceful)` | 全コンポーネントをStop（非同期） |
| `ResetAllAsync()` | 全コンポーネントをReset（非同期） |
| `GetAllComponentStatus()` | 全コンポーネントのステータス取得 |
| `GetJobStatus(job_id)` | 非同期処理の状態取得 |

#### Web API統合

OperatorはOat++ Web APIサーバー内で動作し、REST API経由で制御可能。

```
┌─────────────────────────────────────────────────┐
│              Oat++ Web API Server               │
│                                                 │
│  ┌─────────────────────────────────────────┐   │
│  │              Operator                    │   │
│  │  - Component管理                         │   │
│  │  - コマンド送信スレッド                   │   │
│  │  - ステータス受信スレッド                 │   │
│  └─────────────────────────────────────────┘   │
│                                                 │
│  REST API:                                      │
│  POST /api/configure  → job_id                  │
│  POST /api/arm        → job_id                  │
│  POST /api/start      → job_id                  │
│  POST /api/stop       → job_id                  │
│  GET  /api/status     → 全Component状態         │
│  GET  /api/jobs/{id}  → 非同期処理状態          │
│  WS   /api/ws         → リアルタイム更新        │
│                                                 │
└─────────────────────────────────────────────────┘
           │
           │ HTTP / WebSocket
           ▼
    ┌─────────────┐
    │   Web UI    │
    └─────────────┘
```

#### 設計上の注意点

| 項目 | 対策 |
|------|------|
| スレッドセーフ | Operatorのメソッドはスレッドセーフに実装 |
| 長時間処理 | 非同期（job_id方式）で即座にACK返却 |
| リアルタイム更新 | WebSocketでステータス変更をpush |
| サーバー停止時 | シグナルハンドラでEmergencyStop |

#### 初期実装

1. 最初はターミナル対話（CLIOperator）で実装・テスト
2. その後Oat++ Web APIサーバーに統合

---

## 2. 状態遷移

### 2.1 状態一覧

| 状態 | 説明 |
|------|------|
| Idle | 初期状態、未設定 |
| Configuring | 設定処理中（過渡状態） |
| Configured | 設定完了、Arm可能 |
| Arming | Arm処理中（過渡状態） |
| Armed | トリガー待ち |
| Starting | Start処理中（過渡状態） |
| Running | データ取得中 |
| Stopping | Stop処理中（過渡状態） |
| Error | エラー発生 |

### 2.2 状態遷移図

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

### 2.3 Armed状態の目的

マスタートリガーによる同期開始：

```
1. 全Component: Configure完了
2. 全Component: Arm（スレーブがトリガー待ち状態に）
3. Operator: 全員Armedを確認
4. マスター: Start（トリガー発行）
5. 全Component: Running
```

---

## 3. スレッド構成

### 3.1 コンポーネント別スレッド一覧

データ取得/送信とデータ処理を分離し、Queueでバッファリングする。
これによりハードウェアバッファ溢れを防止する。

#### Source（5スレッド）

| スレッド | 役割 |
|----------|------|
| メインスレッド | 状態管理 |
| コマンド受信スレッド | Operatorからのコマンド受信、内部状態変更 |
| ステータス送信スレッド | 定期的なステータス報告 |
| データ取得スレッド | Digitizer → Queue |
| データ送信スレッド | Queue → シリアライズ → Network |

```
┌────────────┐      ┌───────┐      ┌────────────┐
│ Digitizer  │─────▶│ Queue │─────▶│ Serialize  │─────▶ Network
│ (HW読出し)  │      │       │      │ + Send     │
└────────────┘      └───────┘      └────────────┘
  取得スレッド                        送信スレッド
```

#### Sink（5スレッド）

| スレッド | 役割 |
|----------|------|
| メインスレッド | 状態管理 |
| コマンド受信スレッド | Operatorからのコマンド受信、内部状態変更 |
| ステータス送信スレッド | 定期的なステータス報告 |
| データ受信スレッド | Network → デシリアライズ → Queue |
| データ書込スレッド | Queue → File |

```
Network ─────▶ ┌─────────────┐      ┌───────┐      ┌────────────┐
               │ Receive +   │─────▶│ Queue │─────▶│ Write File │
               │ Deserialize │      │       │      │            │
               └─────────────┘      └───────┘      └────────────┘
                 受信スレッド                         書込スレッド
```

#### Merger（6スレッド）

| スレッド | 役割 |
|----------|------|
| メインスレッド | 状態管理 |
| コマンド受信スレッド | Operatorからのコマンド受信、内部状態変更 |
| ステータス送信スレッド | 定期的なステータス報告 |
| データ受信スレッド | Network（複数入力をpoll）→ デシリアライズ → Queue |
| マージスレッド | Queue → 時系列ソート → Queue |
| データ送信スレッド | Queue → シリアライズ → Network |

```
Network ─────▶ ┌─────────────┐      ┌───────┐      ┌───────┐      ┌───────┐      ┌────────────┐
(poll複数)     │ Receive +   │─────▶│ Queue │─────▶│ Merge │─────▶│ Queue │─────▶│ Serialize  │─────▶ Network
               │ Deserialize │      │       │      │ Sort  │      │       │      │ + Send     │
               └─────────────┘      └───────┘      └───────┘      └───────┘      └────────────┘
                 受信スレッド                       マージスレッド                   送信スレッド
```

#### Operator（3スレッド）

| スレッド | 役割 |
|----------|------|
| メインスレッド | 状態管理、UI処理 |
| コマンド送信スレッド | Component へのコマンド送信 |
| ステータス受信スレッド | 全Componentのステータス監視 |

### 3.2 スレッド間通信

| 対象 | 方式 | 理由 |
|------|------|------|
| 状態（ComponentState） | `std::atomic` | 高頻度アクセス、単純な読み書き |
| コマンド | `std::mutex` + `std::optional` | 低頻度、複合操作 |
| データ（スレッド間） | `ThreadSafeQueue` | 非同期バッファリング |

### 3.3 Queue管理

- バッファ閾値を設定
- 閾値超過でエラー → 全体停止
- Queueサイズは設定可能

### 3.4 メインループ概念

```cpp
void Component::Run() {
    while (true) {
        switch (fState.load()) {
            case Idle:
                WaitForCommand();
                break;
            case Configured:
                WaitForCommand();
                break;
            case Armed:
                WaitForTrigger();
                break;
            case Running:
                // データ処理はワーカースレッドが実行
                // メインスレッドは状態監視
                MonitorWorkers();
                break;
            case Error:
                WaitForReset();
                break;
            // 過渡状態はコマンドスレッドが処理
        }
    }
}
```

---

## 4. 通信

### 4.1 通信チャネル

| チャネル | パターン | 用途 |
|----------|----------|------|
| コマンド | REQ/REP | Operator → Component 制御 |
| データ | PUSH/PULL または PUB/SUB | Component間データ転送 |
| ステータス | PUB/SUB | Component → Operator 状態報告 |

### 4.2 コマンドプロトコル

ACK + ポーリング方式：

```
Operator ──[Configure]──▶ Component
         ◀──[ACK: received]────────   （即座に返答）
                                      （設定処理中...）
Operator ──[GetStatus]──▶ Component
         ◀──[Status: Configuring]──

Operator ──[GetStatus]──▶ Component
         ◀──[Status: Configured]──    （完了）
```

### 4.3 エラー時の処理

| 状況 | 対応 |
|------|------|
| Arm失敗（1つでも） | 全ComponentをReset |
| Start未達（ポーリングで検出） | 再送を数回試行 |
| バッファ溢れ | エラーとして全体停止 |
| ネットワーク切断 | Component継続、Operator UIにエラー表示 |

---

## 5. 停止シーケンス

### 5.1 GracefulStop

上流から順番に停止（データを流しきってから停止）：

```
1. Operator → Source: Stop
2. Source: データ取得停止 → 残りをフラッシュ → EOS送信 → 停止完了
3. Merger: EOS受信 → 残りをフラッシュ → EOS送信 → 停止完了
4. Sink: EOS受信 → 残りをフラッシュ → 停止完了
```

### 5.2 EmergencyStop

全コンポーネント同時停止：

```
Operator → All: Stop（同時送信）
（データロスの可能性あり）
```

### 5.3 シグナル処理（SIGINT/SIGTERM）

EmergencyStopを実行して即座に終了。

---

## 6. 設定

### 6.1 設定ソース

- JSONファイル
- MongoDB

### 6.2 設定の分担

| 設定 | 管理者 |
|------|--------|
| 全体構成（Componentアドレス一覧） | Operator |
| 個別設定（HW設定、入出力先等） | 各Component |

### 6.3 コンポーネント発見

設定ファイルに全Componentのアドレスを記述：

```json
{
  "components": [
    {"id": "source_01", "command_address": "tcp://192.168.1.10:5555"},
    {"id": "source_02", "command_address": "tcp://192.168.1.11:5555"},
    {"id": "merger_01", "command_address": "tcp://192.168.1.20:5555"},
    {"id": "writer_01", "command_address": "tcp://192.168.1.30:5555"}
  ]
}
```

---

## 7. ラン管理

### 7.1 ラン番号

- ユーザーがStart時に設定
- Operatorがメモリ保持
- 各Componentに配布（Startコマンドに含める）
- 永続化は実装に任せる（ファイル、DB等）

### 7.2 用途

- ファイル名

---

## 8. ログとイベント

### 8.1 ログ

- 出力先: 標準出力（stdout）
- 保存: ファイル + logrotate
- 形式: 標準化

```
[ISO時刻] [レベル] [component_id] メッセージ
[2024-01-15T10:30:00.123Z] [INFO] [source_01] Started acquisition
```

### 8.2 重要イベント（DBに保存）

| イベント | 説明 |
|----------|------|
| RUN_START | ラン開始 |
| RUN_STOP | ラン終了 |
| ERROR | エラー発生 |
| CONFIG_CHANGE | 設定変更 |

```json
{
  "timestamp": "2024-01-15T10:30:00Z",
  "type": "RUN_START",
  "run_number": 100,
  "component_id": "operator_01",
  "details": {}
}
```

---

## 9. 同期とタイミング

### 9.1 タイムスタンプ

- NTP同期（ミリ秒精度）
- EventDataのタイムスタンプはデジタイザが付与（別管理）

### 9.2 同期プロトコル

```
Phase 1: Arm
  Operator → 全Component: ArmCommand
  各Component → Operator: ArmResponse
  失敗があれば全てReset

Phase 2: Verify
  Operator: 全員Armedを確認

Phase 3: Start
  Operator → 全Component: StartCommand
  ポーリングで確認、未達なら再送

Phase 4: Verify Running
  Operator: 全員Runningを確認
  タイムアウトでEmergencyStop
```

---

## 10. 非機能要求

### 10.1 性能目標

| 項目 | 目標値 |
|------|--------|
| データスループット | >= 100 MB/s |
| イベントレート | >= 100,000 events/s |
| コマンド応答時間 | < 100 ms（ACK） |

### 10.2 信頼性

| 項目 | 要求 |
|------|------|
| データロス | 正常動作時は0% |
| バッファ溢れ | 閾値超えでエラー停止 |
| ネットワーク切断 | Component継続、Operator通知 |

### 10.3 保守性

| 項目 | 要求 |
|------|------|
| テストカバレッジ | >= 80% |
| ドキュメント | 全インターフェースにコメント |

---

## 11. 制約条件

| 制約 | 詳細 |
|------|------|
| 言語 | C++17 |
| ビルド | CMake 3.15以上 |
| プラットフォーム | Linux, macOS |
| ネットワーク | ZeroMQ |
| 既存ライブラリ | lib/digitizer, lib/net を活用 |

---

## 12. 用語定義

| 用語 | 定義 |
|------|------|
| Component | DAQシステムを構成する独立した処理単位 |
| DataComponent | データの入出力を行うComponent |
| Operator | 全Componentを制御するComponent |
| EventData | 1つの検出イベントを表すデータ構造 |
| EOS | End-of-Stream（データ終端マーカー） |
| Run | 一連のデータ取得セッション |
| GracefulStop | データをフラッシュしてから停止 |
| EmergencyStop | 即座に停止（データロスの可能性） |
