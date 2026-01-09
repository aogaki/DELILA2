# 残タスク一覧

## 概要

コンポーネントフレームワークのPhase 1-5は完了しているが、以下の機能が未実装または部分実装の状態。

---

## 1. TimeSortMerger 時間ソートロジック（最重要）

**優先度**: 高
**影響**: 複数ソースからのデータマージが動作しない

### 未実装箇所

| ファイル | 関数 | 状態 |
|----------|------|------|
| TimeSortMerger.cpp:397-410 | `ReceivingLoop()` | マージバッファへの追加が未実装 |
| TimeSortMerger.cpp:413-421 | `MergingLoop()` | 空のループ（ソートロジックなし） |
| TimeSortMerger.cpp:423-428 | `SendingLoop()` | 空のループ（送信ロジックなし） |
| TimeSortMerger.cpp:402-405 | EOS追跡 | `fEOSTracker`未使用 |

### 実装が必要な機能

1. **マージバッファ**
   - スレッドセーフなバッファ構造
   - 入力ソースごとのイベント格納
   - タイムスタンプでソート可能な構造

2. **時間ソートアルゴリズム**
   - 各入力からのイベントを時系列順にマージ
   - ソートウィンドウ（`fSortWindowNs`）の適用
   - 最新タイムスタンプ - ソートウィンドウより古いイベントを出力

3. **出力送信**
   - ソート済みイベントのシリアライズ
   - `fOutputTransport`経由でPUSH送信

4. **EOS追跡**
   - 全入力からEOS受信を追跡
   - 全入力完了後に下流へEOS送信

### 設計案

```cpp
// マージバッファ構造
struct TaggedEvent {
    size_t source_index;
    std::unique_ptr<Digitizer::EventData> event;
};

// 時間順ソート用の優先度キュー
std::priority_queue<TaggedEvent, std::vector<TaggedEvent>, TimestampComparator> fMergeBuffer;
std::mutex fMergeBufferMutex;

// ソートウィンドウの適用
void MergingLoop() {
    while (fRunning) {
        std::lock_guard<std::mutex> lock(fMergeBufferMutex);

        if (fMergeBuffer.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // 最新タイムスタンプを取得
        double newest_timestamp = GetNewestTimestamp();
        double output_threshold = newest_timestamp - fSortWindowNs;

        // 閾値より古いイベントを出力キューへ
        while (!fMergeBuffer.empty() &&
               fMergeBuffer.top().event->timeStampNs < output_threshold) {
            // 出力キューに追加
            fOutputQueue.push(std::move(fMergeBuffer.top()));
            fMergeBuffer.pop();
        }
    }
}
```

---

## 2. 設定ファイル読込（全コンポーネント共通）

**優先度**: 中
**影響**: 現在はプログラム内でハードコード設定のみ

### 未実装箇所

| ファイル | 行 | コメント |
|----------|-----|----------|
| DigitizerSource.cpp:29 | `// TODO: Load configuration from file` |
| FileWriter.cpp:31 | `// TODO: Load configuration from file` |
| CLIOperator.cpp:27 | `// TODO: Load configuration from file if needed` |

### 実装が必要な機能

1. **JSON設定ファイルパーサー**
   - nlohmann/json使用（既にプロジェクトに含まれている可能性）
   - 各コンポーネント固有の設定項目

2. **設定項目**

   **DigitizerSource**:
   ```json
   {
     "component_id": "source_01",
     "mock_mode": true,
     "mock_event_rate": 1000,
     "output_address": "tcp://*:5555",
     "command_address": "tcp://*:5556"
   }
   ```

   **FileWriter**:
   ```json
   {
     "component_id": "writer_01",
     "input_address": "tcp://localhost:5555",
     "output_path": "/data/runs",
     "file_prefix": "run_",
     "command_address": "tcp://*:5557"
   }
   ```

   **TimeSortMerger**:
   ```json
   {
     "component_id": "merger_01",
     "input_addresses": ["tcp://localhost:5555", "tcp://localhost:5556"],
     "output_address": "tcp://*:5560",
     "sort_window_ns": 10000000,
     "command_address": "tcp://*:5561"
   }
   ```

   **CLIOperator**:
   ```json
   {
     "operator_id": "operator_01",
     "components": [
       {"id": "source_01", "command_address": "tcp://192.168.1.10:5556"},
       {"id": "writer_01", "command_address": "tcp://192.168.1.20:5557"}
     ]
   }
   ```

---

## 3. キューベースのスレッド分離

**優先度**: 低
**影響**: 高負荷時のパフォーマンス（現状でも動作する）

### 未実装箇所

| ファイル | 関数 | コメント |
|----------|------|----------|
| DigitizerSource.cpp:289-294 | `SendingLoop()` | `// TODO: Implement queue-based sending` |
| FileWriter.cpp:334-339 | `WritingLoop()` | `// TODO: Implement queue-based writing for decoupling` |

### 現状

- DigitizerSource: `AcquisitionLoop()`内で直接送信
- FileWriter: `ReceivingLoop()`内で直接書込

### 実装が必要な機能

1. **ThreadSafeQueue**
   - 既存の`lib/net`に実装があれば再利用
   - なければ新規実装

2. **キューベースアーキテクチャ**
   ```
   [Acquisition] → [Queue] → [Sending]
   [Receiving] → [Queue] → [Writing]
   ```

3. **メリット**
   - ハードウェアバッファ溢れ防止
   - I/O待ちによるブロッキング回避
   - バッファサイズ監視・警告

---

## 4. 実デジタイザハードウェア統合

**優先度**: 高（実運用時）
**影響**: 実ハードウェアからのデータ取得

### 未実装箇所

| ファイル | 行 | コメント |
|----------|-----|----------|
| DigitizerSource.cpp:283-284 | `// TODO: Read from actual digitizer` |

### 実装が必要な機能

1. **IDigitizer インターフェース統合**
   - `lib/digitizer`の既存実装を使用
   - DigitizerSourceのコンストラクタでIDigitizerを注入

2. **実装例**
   ```cpp
   void DigitizerSource::AcquisitionLoop() {
       while (fRunning) {
           if (fMockMode) {
               GenerateMockEvents();
           } else {
               // 実デジタイザからデータ読出し
               auto events = fDigitizer->ReadData();
               if (events && !events->empty()) {
                   auto data = fDataProcessor->ProcessWithAutoSequence(events);
                   if (data && fTransport->IsConnected()) {
                       fTransport->SendBytes(data);
                       fEventsProcessed += events->size();
                       fBytesTransferred += data->size();
                   }
               }
           }
       }
   }
   ```

---

## 優先順位

| 順位 | タスク | 理由 |
|------|--------|------|
| 1 | TimeSortMerger時間ソート | 複数ソースのマージが動作しない |
| 2 | 実デジタイザ統合 | 実運用に必須 |
| 3 | 設定ファイル読込 | 運用時の柔軟性向上 |
| 4 | キューベース分離 | パフォーマンス最適化（後回し可） |

---

## 関連ドキュメント

- [00_OVERVIEW.md](00_OVERVIEW.md) - 全体概要
- [03_IMPLEMENTATION_PLAN.md](03_IMPLEMENTATION_PLAN.md) - 実装計画
- [CLAUDE.md](../CLAUDE.md) - 開発ガイドライン（TDD原則）
