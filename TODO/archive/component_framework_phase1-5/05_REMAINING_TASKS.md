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

### 設計方針

**一括ソート方式を採用**（KISS原則）

- 全入力からEOSを受信するまでイベントを収集
- ラン終了時に全イベントをタイムスタンプでソート
- ソート済みデータを下流へ一括送信

**理由**:
- 主な用途はオフライン解析（FileWriter）向けのファイル書き込み
- リアルタイムソート（タイムウィンドウ方式）はOnlineAnalyzerで実装予定
- 実装がシンプルで確実にソートされる

### 実装が必要な機能

1. **イベント収集バッファ**
   - 各ReceivingLoopからイベントを受け取るキュー
   - スレッドセーフな追加操作

2. **一括ソートロジック**
   - 全入力からEOS受信後にソート実行
   - `std::sort`でタイムスタンプ順に並べ替え

3. **出力送信**
   - ソート済みイベントのシリアライズ
   - `fOutputTransport`経由でPUSH送信
   - 送信完了後にEOSを送信

4. **EOS追跡**
   - `fEOSTracker`を使用して全入力のEOS受信を追跡
   - 全入力完了を検知してソート・送信を開始

### 設計案

```cpp
// イベント収集用の構造
struct TaggedEvent {
    size_t source_index;
    std::unique_ptr<Digitizer::EventData> event;
};

// メンバ変数
std::vector<TaggedEvent> fCollectedEvents;
std::mutex fCollectedEventsMutex;

// ReceivingLoopでイベントを収集
void ReceivingLoop(size_t input_index) {
    while (fRunning) {
        auto data = fInputTransports[input_index]->ReceiveBytes();

        if (Net::DataProcessor::IsEOSMessage(data->data(), data->size())) {
            fEOSTracker->ReceiveEOS("input_" + std::to_string(input_index));
            break;
        }

        auto [events, sequence] = fDataProcessor->Decode(data);
        if (events && !events->empty()) {
            std::lock_guard<std::mutex> lock(fCollectedEventsMutex);
            for (auto& event : *events) {
                fCollectedEvents.push_back({input_index, std::move(event)});
            }
            fEventsProcessed += events->size();
        }
    }
}

// MergingLoop（実際はラン終了時に1回実行）
void MergingLoop() {
    // 全入力からEOS受信を待機
    while (fRunning && !fEOSTracker->AllEOSReceived()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!fRunning) return;

    // 全イベントをタイムスタンプでソート
    {
        std::lock_guard<std::mutex> lock(fCollectedEventsMutex);
        std::sort(fCollectedEvents.begin(), fCollectedEvents.end(),
            [](const auto& a, const auto& b) {
                return a.event->timeStampNs < b.event->timeStampNs;
            });
    }

    // ソート済みイベントを送信
    SendAllEvents();

    // EOS送信
    fOutputTransport->SendEOS();
}
```

### 注意事項

- `fSortWindowNs`は現在の実装では使用しない（将来のOnlineAnalyzer用に残す）
- メモリ使用量はラン中の全イベント数に比例（大規模ラン時は注意）

---

## 2. OnlineAnalyzer タイムウィンドウ方式（将来実装）

**優先度**: 低（現時点では未実装予定）
**影響**: リアルタイム解析が必要な場合のみ

### 概要

オンライン解析用にリアルタイムでソートされたデータストリームが必要な場合、
タイムウィンドウ方式をOnlineAnalyzerコンポーネントに実装する。

### 設計方針

```
[Source1] ──┐
            ├──▶ [TimeSortMerger] ──▶ [OnlineAnalyzer]
[Source2] ──┘                              │
                                           ├── タイムウィンドウ適用
                                           ├── リアルタイムソート
                                           └── 解析処理
```

### タイムウィンドウ方式の実装案

```cpp
class OnlineAnalyzer : public IDataComponent {
private:
    uint64_t fTimeWindowNs = 10000000;  // 10ms デフォルト
    std::priority_queue<Event, std::vector<Event>, TimestampComparator> fSortBuffer;

    void ProcessingLoop() {
        while (fRunning) {
            auto event = ReceiveEvent();
            fSortBuffer.push(event);

            // ウィンドウ外のイベントを処理
            double threshold = GetNewestTimestamp() - fTimeWindowNs;
            while (!fSortBuffer.empty() &&
                   fSortBuffer.top().timeStampNs < threshold) {
                AnalyzeEvent(fSortBuffer.top());
                fSortBuffer.pop();
            }
        }
    }
};
```

### メリット

- TimeSortMergerはシンプルな一括ソートに専念
- リアルタイム要件はOnlineAnalyzerで処理
- 各コンポーネントの責務が明確

---

## 3. 設定ファイル読込（全コンポーネント共通）

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

## 4. キューベースのスレッド分離

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

## 5. 実デジタイザハードウェア統合

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
| 1 | TimeSortMerger一括ソート | 複数ソースのマージが動作しない |
| 2 | 実デジタイザ統合 | 実運用に必須 |
| 3 | 設定ファイル読込 | 運用時の柔軟性向上 |
| 4 | キューベース分離 | パフォーマンス最適化（後回し可） |
| 5 | OnlineAnalyzerタイムウィンドウ | リアルタイム解析が必要な場合のみ |

---

## 関連ドキュメント

- [00_OVERVIEW.md](00_OVERVIEW.md) - 全体概要
- [03_IMPLEMENTATION_PLAN.md](03_IMPLEMENTATION_PLAN.md) - 実装計画
- [CLAUDE.md](../CLAUDE.md) - 開発ガイドライン（TDD原則）
