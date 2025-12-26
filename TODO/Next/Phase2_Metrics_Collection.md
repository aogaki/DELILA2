# Phase 2: Metrics収集

**優先度**: 高
**目的**: コンポーネントの性能・状態メトリクスを収集・送信する機能を実装
**送信頻度**: 定期送信（1秒ごと） + オンデマンド

---

## タスク一覧

### M-1: Metrics構造体定義
**ファイル**: `lib/net/include/Metrics.hpp`
**見積もり**: 30分
**依存**: なし

#### 実装内容
```cpp
// lib/net/include/Metrics.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace DELILA::Net {

/**
 * @brief コンポーネントのパフォーマンスメトリクス
 *
 * スループット、バッファ状態、レイテンシ、エラー統計を含む
 */
struct Metrics {
    // === スループット ===
    double events_per_second = 0.0;    // 1秒あたりのイベント数
    double bytes_per_second = 0.0;     // 1秒あたりのバイト数
    uint64_t total_events = 0;         // 累計イベント数
    uint64_t total_bytes = 0;          // 累計バイト数

    // === バッファ状態 ===
    uint64_t queue_size = 0;           // 現在のキューサイズ
    uint64_t queue_capacity = 0;       // キューの最大容量
    uint64_t dropped_events = 0;       // 欠落イベント数
    double queue_utilization = 0.0;    // キュー使用率 (0.0-1.0)

    // === レイテンシ ===
    double avg_latency_us = 0.0;       // 平均レイテンシ（マイクロ秒）
    double min_latency_us = 0.0;       // 最小レイテンシ（マイクロ秒）
    double max_latency_us = 0.0;       // 最大レイテンシ（マイクロ秒）
    double p99_latency_us = 0.0;       // 99パーセンタイルレイテンシ

    // === エラー統計 ===
    uint64_t error_count = 0;          // エラー発生回数
    uint64_t retry_count = 0;          // リトライ回数
    uint64_t timeout_count = 0;        // タイムアウト回数

    // === 稼働情報 ===
    uint64_t uptime_seconds = 0;       // 稼働時間（秒）
    std::chrono::system_clock::time_point start_time;  // 開始時刻

    // === シーケンス ===
    uint64_t sequence_gaps = 0;        // シーケンスギャップ数
    uint64_t last_sequence = 0;        // 最後のシーケンス番号

    // === ヘルパーメソッド ===
    void Reset();                      // 全メトリクスをリセット
    void UpdateQueueUtilization();     // キュー使用率を計算
};

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] デフォルト値が全て0または空
- [ ] Reset()で全値が初期化される
- [ ] UpdateQueueUtilization()が正しく計算する
- [ ] queue_capacity=0の場合にutilization=0

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] 実装ファイル作成
- [ ] ユニットテスト作成・パス

---

### M-2: MetricsCollectorクラス実装
**ファイル**: `lib/net/include/MetricsCollector.hpp`, `lib/net/src/MetricsCollector.cpp`
**見積もり**: 3時間
**依存**: M-1

#### 実装内容
```cpp
// lib/net/include/MetricsCollector.hpp
#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>

#include "Metrics.hpp"

namespace DELILA::Net {

/**
 * @brief スレッドセーフなメトリクス収集クラス
 *
 * イベント処理中にメトリクスを記録し、
 * 定期的に集計結果を取得できる
 */
class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector() = default;

    // === イベント記録（スレッドセーフ） ===

    /**
     * @brief イベント処理を記録
     * @param event_count 処理したイベント数
     * @param bytes 処理したバイト数
     */
    void RecordEvents(uint64_t event_count, uint64_t bytes);

    /**
     * @brief レイテンシを記録
     * @param latency 処理レイテンシ
     */
    void RecordLatency(std::chrono::microseconds latency);

    /**
     * @brief エラーを記録
     */
    void RecordError();

    /**
     * @brief リトライを記録
     */
    void RecordRetry();

    /**
     * @brief タイムアウトを記録
     */
    void RecordTimeout();

    /**
     * @brief シーケンスギャップを記録
     * @param gap_size ギャップのサイズ
     */
    void RecordSequenceGap(uint64_t gap_size = 1);

    /**
     * @brief イベントドロップを記録
     * @param count ドロップしたイベント数
     */
    void RecordDroppedEvents(uint64_t count);

    // === キュー状態更新 ===

    /**
     * @brief キュー状態を更新
     * @param current_size 現在のキューサイズ
     * @param capacity キューの最大容量
     */
    void SetQueueState(uint64_t current_size, uint64_t capacity);

    /**
     * @brief シーケンス番号を更新
     * @param sequence 最新のシーケンス番号
     */
    void SetLastSequence(uint64_t sequence);

    // === メトリクス取得 ===

    /**
     * @brief 現在のメトリクスを取得
     * @return 集計されたメトリクス
     *
     * スループットは直近1秒間の平均値
     * レイテンシは統計値（平均、最小、最大、p99）
     */
    Metrics GetMetrics() const;

    /**
     * @brief メトリクスをリセット
     *
     * 累計値はリセットされない（total_events, total_bytes）
     * エラーカウントもリセットされない
     */
    void ResetPeriodic();

    /**
     * @brief 全メトリクスを完全リセット
     */
    void ResetAll();

private:
    // アトミックカウンター（高速更新用）
    std::atomic<uint64_t> event_count_{0};
    std::atomic<uint64_t> byte_count_{0};
    std::atomic<uint64_t> total_events_{0};
    std::atomic<uint64_t> total_bytes_{0};
    std::atomic<uint64_t> error_count_{0};
    std::atomic<uint64_t> retry_count_{0};
    std::atomic<uint64_t> timeout_count_{0};
    std::atomic<uint64_t> sequence_gaps_{0};
    std::atomic<uint64_t> dropped_events_{0};
    std::atomic<uint64_t> last_sequence_{0};

    // キュー状態
    std::atomic<uint64_t> queue_size_{0};
    std::atomic<uint64_t> queue_capacity_{0};

    // レイテンシ統計（ミューテックス保護）
    mutable std::mutex latency_mutex_;
    std::deque<double> latency_samples_;  // 直近N個のサンプル
    static constexpr size_t MAX_LATENCY_SAMPLES = 1000;

    // 時間管理
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_reset_time_;

    // ヘルパー
    double CalculatePercentile(double percentile) const;
};

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] RecordEvents()がスレッドセーフに動作する
- [ ] RecordLatency()がサンプルを正しく保存する
- [ ] GetMetrics()がevents_per_secondを正しく計算する
- [ ] GetMetrics()がbytes_per_secondを正しく計算する
- [ ] GetMetrics()がavg_latency_usを正しく計算する
- [ ] GetMetrics()がmax_latency_usを正しく計算する
- [ ] GetMetrics()がp99_latency_usを正しく計算する
- [ ] SetQueueState()がqueue_utilizationを正しく計算する
- [ ] ResetPeriodic()が累計値を保持する
- [ ] ResetAll()が全値をリセットする
- [ ] MAX_LATENCY_SAMPLESを超えると古いサンプルが削除される
- [ ] 並行アクセステスト（複数スレッドから同時更新）

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] 実装ファイル作成
- [ ] ユニットテスト作成・パス (`tests/unit/net/test_metrics_collector.cpp`)

---

### M-3: ComponentStatusにMetrics統合
**ファイル**: `lib/net/include/ZMQTransport.hpp`
**見積もり**: 1時間
**依存**: M-2

#### 実装内容
```cpp
// ZMQTransport.hpp のComponentStatus構造体を更新

#include "Metrics.hpp"

/**
 * @brief コンポーネントステータス（メトリクス統合版）
 */
struct ComponentStatus {
    std::string component_id;                         // コンポーネントID
    std::string group_id;                             // グループID
    ComponentState state = ComponentState::Loaded;    // 現在の状態
    std::chrono::system_clock::time_point timestamp;  // タイムスタンプ
    uint64_t heartbeat_counter = 0;                   // ハートビートカウンター
    std::string error_message;                        // 最新エラーメッセージ
    ErrorCode last_error = ErrorCode::SUCCESS;        // 最新エラーコード

    // メトリクス（詳細版）
    Metrics metrics;

    // 後方互換性のためのメトリクスマップ（非推奨）
    std::map<std::string, double> legacy_metrics;
};
```

#### シリアライズ更新
```cpp
// Metrics JSONフォーマット
{
    "component_id": "digitizer_01",
    "group_id": "sources",
    "state": "Running",
    "timestamp": "2024-01-15T10:30:00.789Z",
    "heartbeat_counter": 3600,
    "error_message": "",
    "last_error": 0,
    "metrics": {
        "events_per_second": 125000.5,
        "bytes_per_second": 12500000.0,
        "total_events": 450000000,
        "total_bytes": 45000000000,
        "queue_size": 1024,
        "queue_capacity": 10240,
        "queue_utilization": 0.1,
        "dropped_events": 0,
        "avg_latency_us": 150.5,
        "min_latency_us": 50.0,
        "max_latency_us": 2500.0,
        "p99_latency_us": 1200.0,
        "error_count": 0,
        "retry_count": 0,
        "timeout_count": 0,
        "uptime_seconds": 3600,
        "sequence_gaps": 0,
        "last_sequence": 450000000
    }
}
```

#### テスト項目
- [ ] ComponentStatusにMetricsが正しく含まれる
- [ ] シリアライズでMetricsの全フィールドが出力される
- [ ] デシリアライズでMetricsの全フィールドが復元される
- [ ] 後方互換性: legacy_metricsも読み取り可能

#### 完了条件
- [ ] ComponentStatus構造体更新
- [ ] シリアライズ/デシリアライズ更新
- [ ] ユニットテスト更新・パス

---

### M-4: 定期送信機構実装
**ファイル**: `lib/net/include/MetricsReporter.hpp`, `lib/net/src/MetricsReporter.cpp`
**見積もり**: 2時間
**依存**: M-3

#### 実装内容
```cpp
// lib/net/include/MetricsReporter.hpp
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "MetricsCollector.hpp"
#include "ZMQTransport.hpp"

namespace DELILA::Net {

/**
 * @brief メトリクス定期送信クラス
 *
 * 指定間隔でMetricsCollectorからメトリクスを取得し、
 * ZMQTransportを通じて送信する
 */
class MetricsReporter {
public:
    /**
     * @brief コンストラクタ
     * @param component_id コンポーネントID
     * @param collector メトリクスコレクター（共有）
     * @param transport トランスポート（共有）
     */
    MetricsReporter(
        const std::string& component_id,
        std::shared_ptr<MetricsCollector> collector,
        std::shared_ptr<ZMQTransport> transport);

    ~MetricsReporter();

    // コピー禁止
    MetricsReporter(const MetricsReporter&) = delete;
    MetricsReporter& operator=(const MetricsReporter&) = delete;

    /**
     * @brief 定期送信開始
     * @param interval 送信間隔（デフォルト1秒）
     */
    void Start(std::chrono::milliseconds interval = std::chrono::seconds(1));

    /**
     * @brief 定期送信停止
     */
    void Stop();

    /**
     * @brief 即座にメトリクスを送信（オンデマンド）
     * @return 送信成功/失敗
     */
    bool SendNow();

    /**
     * @brief グループIDを設定
     */
    void SetGroupId(const std::string& group_id);

    /**
     * @brief 現在の状態を設定
     */
    void SetState(ComponentState state);

    /**
     * @brief エラーメッセージを設定
     */
    void SetError(ErrorCode code, const std::string& message);

    /**
     * @brief エラーをクリア
     */
    void ClearError();

    /**
     * @brief 送信中かどうか
     */
    bool IsRunning() const;

private:
    void ReportingLoop();
    ComponentStatus BuildStatus() const;

    std::string component_id_;
    std::string group_id_;
    std::atomic<ComponentState> state_{ComponentState::Loaded};
    std::atomic<ErrorCode> last_error_{ErrorCode::SUCCESS};
    std::string error_message_;
    mutable std::mutex error_mutex_;

    std::shared_ptr<MetricsCollector> collector_;
    std::shared_ptr<ZMQTransport> transport_;

    std::atomic<bool> running_{false};
    std::chrono::milliseconds interval_{1000};
    std::thread reporter_thread_;
    std::atomic<uint64_t> heartbeat_counter_{0};
};

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] Start()で送信スレッドが開始される
- [ ] Stop()で送信スレッドが停止される
- [ ] 指定間隔でメトリクスが送信される
- [ ] SendNow()で即座に送信される
- [ ] SetState()で状態が正しく反映される
- [ ] SetError()でエラー情報が正しく反映される
- [ ] heartbeat_counterが毎回インクリメントされる
- [ ] デストラクタでスレッドが正しく終了する

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] 実装ファイル作成
- [ ] ユニットテスト作成・パス

---

### M-5: Metricsユニットテスト
**ファイル**: `tests/unit/net/test_metrics.cpp`, `tests/unit/net/test_metrics_collector.cpp`
**見積もり**: 2時間
**依存**: M-2

#### テストケース一覧

```cpp
// test_metrics.cpp

// === Metrics構造体テスト ===
TEST(MetricsTest, DefaultValues)
TEST(MetricsTest, Reset)
TEST(MetricsTest, UpdateQueueUtilization)
TEST(MetricsTest, UpdateQueueUtilizationZeroCapacity)

// test_metrics_collector.cpp

// === 基本機能テスト ===
TEST(MetricsCollectorTest, InitialState)
TEST(MetricsCollectorTest, RecordSingleEvent)
TEST(MetricsCollectorTest, RecordMultipleEvents)
TEST(MetricsCollectorTest, RecordLatency)
TEST(MetricsCollectorTest, RecordError)
TEST(MetricsCollectorTest, RecordRetry)
TEST(MetricsCollectorTest, RecordTimeout)
TEST(MetricsCollectorTest, RecordSequenceGap)
TEST(MetricsCollectorTest, RecordDroppedEvents)
TEST(MetricsCollectorTest, SetQueueState)
TEST(MetricsCollectorTest, SetLastSequence)

// === スループット計算テスト ===
TEST(MetricsCollectorTest, EventsPerSecondCalculation)
TEST(MetricsCollectorTest, BytesPerSecondCalculation)
TEST(MetricsCollectorTest, TotalEventsAccumulation)
TEST(MetricsCollectorTest, TotalBytesAccumulation)

// === レイテンシ統計テスト ===
TEST(MetricsCollectorTest, AverageLatency)
TEST(MetricsCollectorTest, MinLatency)
TEST(MetricsCollectorTest, MaxLatency)
TEST(MetricsCollectorTest, P99Latency)
TEST(MetricsCollectorTest, LatencySampleLimit)

// === リセットテスト ===
TEST(MetricsCollectorTest, ResetPeriodicKeepsTotals)
TEST(MetricsCollectorTest, ResetAllClearsEverything)

// === スレッドセーフティテスト ===
TEST(MetricsCollectorTest, ConcurrentRecordEvents)
TEST(MetricsCollectorTest, ConcurrentRecordLatency)
TEST(MetricsCollectorTest, ConcurrentMixedOperations)

// test_metrics_reporter.cpp

// === MetricsReporterテスト ===
TEST(MetricsReporterTest, StartStop)
TEST(MetricsReporterTest, PeriodicSending)
TEST(MetricsReporterTest, SendNow)
TEST(MetricsReporterTest, StateReflection)
TEST(MetricsReporterTest, ErrorReflection)
TEST(MetricsReporterTest, HeartbeatIncrement)
```

#### 完了条件
- [ ] 全テストケース実装
- [ ] 全テストパス
- [ ] カバレッジ80%以上

---

### M-6: Metrics統合テスト
**ファイル**: `tests/integration/test_metrics_integration.cpp`
**見積もり**: 2時間
**依存**: M-4, M-5

#### テストケース一覧

```cpp
// test_metrics_integration.cpp

class MetricsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // サーバー・クライアント設定
        // MetricsCollector, MetricsReporter, ZMQTransport設定
    }
};

// === エンドツーエンドテスト ===
TEST_F(MetricsIntegrationTest, CollectorToTransport)
TEST_F(MetricsIntegrationTest, PeriodicReporting)
TEST_F(MetricsIntegrationTest, OnDemandReporting)
TEST_F(MetricsIntegrationTest, MetricsReceivedCorrectly)
TEST_F(MetricsIntegrationTest, MultipleReporters)
TEST_F(MetricsIntegrationTest, HighFrequencyUpdates)
TEST_F(MetricsIntegrationTest, LongRunningTest)
```

#### 完了条件
- [ ] 全テストケース実装
- [ ] 全テストパス
- [ ] 実際のZMQソケット間通信で動作確認

---

## チェックリスト

### Phase 2 完了条件

- [ ] M-1: Metrics構造体定義 完了
- [ ] M-2: MetricsCollectorクラス実装 完了
- [ ] M-3: ComponentStatusにMetrics統合 完了
- [ ] M-4: 定期送信機構実装 完了
- [ ] M-5: Metricsユニットテスト 完了
- [ ] M-6: Metrics統合テスト 完了
- [ ] 全テスト合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

---

## 実装順序

```
M-1 ──→ M-2 ──→ M-3 ──→ M-4 ──→ M-5 ──→ M-6
                              ↑
                              └─並行可能─┘
```
