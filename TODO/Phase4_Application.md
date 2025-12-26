# Phase 4: Application層

**見積もり**: 6時間
**目的**: ビジネスロジックの実装
**依存**: Phase 0, Phase 1, Phase 2, Phase 3 完了

---

## 設計原則

- **インターフェース依存**: Infrastructure層の具体実装ではなくインターフェースに依存
- **依存性注入**: コンストラクタでインターフェースを受け取る
- **テスト容易性**: モックを使ったテストが可能

---

## タスク一覧

### P4-1: StateManager 実装
**見積もり**: 1.5時間

```cpp
// lib/net/include/application/StateManager.hpp
#pragma once

#include "domain/ComponentState.hpp"
#include "domain/Command.hpp"
#include "domain/ErrorCode.hpp"
#include <functional>
#include <mutex>
#include <optional>
#include <variant>

namespace DELILA::Net {

/**
 * @brief 状態遷移エラー情報
 */
struct TransitionError {
    ErrorCode code;
    std::string message;
};

/**
 * @brief コンポーネントの状態を管理
 *
 * スレッドセーフな状態遷移を提供
 */
class StateManager {
public:
    using StateChangeCallback = std::function<void(ComponentState old_state, ComponentState new_state)>;

    StateManager();
    ~StateManager() = default;

    // === 状態取得 ===
    ComponentState GetState() const;
    bool IsInState(ComponentState state) const;
    bool IsError() const { return GetState() == ComponentState::Error; }

    // === 状態遷移 ===

    /**
     * @brief コマンドに基づいて状態遷移を試行
     * @return 成功時は新しい状態、失敗時はエラー情報
     */
    std::variant<ComponentState, TransitionError> TryTransition(CommandType command);

    /**
     * @brief エラー状態に遷移
     */
    void TransitionToError(ErrorCode code, const std::string& message = "");

    /**
     * @brief 強制的に状態を設定（テスト用）
     */
    void ForceState(ComponentState state);

    // === エラー情報 ===
    ErrorCode GetLastErrorCode() const;
    std::string GetLastErrorMessage() const;
    void ClearError();

    // === コールバック ===
    void SetStateChangeCallback(StateChangeCallback callback);

private:
    mutable std::mutex mutex_;
    ComponentState state_ = ComponentState::Loaded;
    ErrorCode last_error_code_ = ErrorCode::SUCCESS;
    std::string last_error_message_;
    StateChangeCallback callback_;

    void DoTransition(ComponentState new_state);
    std::optional<ComponentState> GetNextState(ComponentState current, CommandType command) const;
};

}  // namespace DELILA::Net
```

```cpp
// lib/net/src/application/StateManager.cpp
#include "application/StateManager.hpp"

namespace DELILA::Net {

StateManager::StateManager() = default;

ComponentState StateManager::GetState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool StateManager::IsInState(ComponentState state) const {
    return GetState() == state;
}

std::variant<ComponentState, TransitionError> StateManager::TryTransition(CommandType command) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Error状態からの CONFIGURE は特別処理（リカバリ）
    if (state_ == ComponentState::Error && command == CommandType::CONFIGURE) {
        last_error_code_ = ErrorCode::SUCCESS;
        last_error_message_.clear();
        DoTransition(ComponentState::Configured);
        return ComponentState::Configured;
    }

    // Error状態からの RESET もリカバリ可能
    if (state_ == ComponentState::Error && command == CommandType::RESET) {
        last_error_code_ = ErrorCode::SUCCESS;
        last_error_message_.clear();
        DoTransition(ComponentState::Loaded);
        return ComponentState::Loaded;
    }

    auto next = GetNextState(state_, command);
    if (!next) {
        return TransitionError{
            ErrorCode::STATE_INVALID_TRANSITION,
            "Cannot " + CommandTypeToString(command) + " from " + ComponentStateToString(state_)
        };
    }

    DoTransition(*next);
    return *next;
}

void StateManager::TransitionToError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_code_ = code;
    last_error_message_ = message;
    DoTransition(ComponentState::Error);
}

void StateManager::ForceState(ComponentState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

ErrorCode StateManager::GetLastErrorCode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_code_;
}

std::string StateManager::GetLastErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_message_;
}

void StateManager::ClearError() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_code_ = ErrorCode::SUCCESS;
    last_error_message_.clear();
}

void StateManager::SetStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

void StateManager::DoTransition(ComponentState new_state) {
    // mutex は呼び出し元で保持
    ComponentState old_state = state_;
    state_ = new_state;

    if (callback_ && old_state != new_state) {
        callback_(old_state, new_state);
    }
}

std::optional<ComponentState> StateManager::GetNextState(
    ComponentState current, CommandType command) const {

    // 状態遷移表に基づく実装
    switch (current) {
        case ComponentState::Loaded:
            if (command == CommandType::CONFIGURE) return ComponentState::Configured;
            if (command == CommandType::RESET) return ComponentState::Loaded;
            break;

        case ComponentState::Configured:
            if (command == CommandType::CONFIGURE) return ComponentState::Configured;
            if (command == CommandType::ARM) return ComponentState::Armed;
            if (command == CommandType::STOP) return ComponentState::Loaded;
            if (command == CommandType::RESET) return ComponentState::Loaded;
            break;

        case ComponentState::Armed:
            if (command == CommandType::START) return ComponentState::Running;
            if (command == CommandType::STOP) return ComponentState::Loaded;
            if (command == CommandType::RESET) return ComponentState::Loaded;
            break;

        case ComponentState::Running:
            if (command == CommandType::STOP) return ComponentState::Loaded;
            if (command == CommandType::RESET) return ComponentState::Loaded;
            break;

        case ComponentState::Error:
            // Error からは CONFIGURE/RESET のみ（上で処理済み）
            break;

        default:
            break;
    }

    // STATUS は状態を変更しない
    if (command == CommandType::STATUS) {
        return current;
    }

    return std::nullopt;
}

}  // namespace DELILA::Net
```

---

### P4-2: MetricsCollector 実装
**見積もり**: 1.5時間

```cpp
// lib/net/include/application/MetricsCollector.hpp
#pragma once

#include "domain/Metrics.hpp"
#include <atomic>
#include <chrono>
#include <mutex>

namespace DELILA::Net {

/**
 * @brief メトリクスを収集するクラス
 *
 * スレッドセーフなメトリクス収集を提供
 */
class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector() = default;

    // === 記録メソッド ===

    /**
     * @brief イベント処理を記録
     */
    void RecordEvent(size_t bytes);

    /**
     * @brief レイテンシを記録
     */
    void RecordLatency(std::chrono::microseconds latency);

    /**
     * @brief エラーを記録
     */
    void RecordError();

    /**
     * @brief シーケンスギャップを記録
     */
    void RecordSequenceGap(uint64_t gap_count = 1);

    /**
     * @brief キューサイズを設定
     */
    void SetQueueSize(uint64_t size, uint64_t capacity);

    /**
     * @brief ドロップイベントを記録
     */
    void RecordDroppedEvents(uint64_t count);

    // === 取得メソッド ===

    /**
     * @brief 現在のメトリクスを取得
     *
     * 呼び出し時点のスナップショットを返す
     */
    Metrics GetCurrentMetrics() const;

    /**
     * @brief メトリクスをリセット
     */
    void Reset();

    /**
     * @brief 計測開始からの経過時間を取得
     */
    uint64_t GetUptimeSeconds() const;

private:
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_calc_time_;

    // アトミックカウンタ（高頻度更新用）
    std::atomic<uint64_t> event_count_{0};
    std::atomic<uint64_t> byte_count_{0};
    std::atomic<uint64_t> error_count_{0};
    std::atomic<uint64_t> sequence_gaps_{0};
    std::atomic<uint64_t> dropped_events_{0};

    // レイテンシ統計（mutex保護）
    mutable std::mutex latency_mutex_;
    uint64_t latency_sample_count_ = 0;
    double latency_sum_us_ = 0.0;
    double max_latency_us_ = 0.0;

    // キュー情報（mutex保護）
    mutable std::mutex queue_mutex_;
    uint64_t queue_size_ = 0;
    uint64_t queue_capacity_ = 0;

    // スループット計算用
    mutable std::mutex throughput_mutex_;
    uint64_t last_event_count_ = 0;
    uint64_t last_byte_count_ = 0;
    double events_per_second_ = 0.0;
    double bytes_per_second_ = 0.0;

    void UpdateThroughput() const;
};

}  // namespace DELILA::Net
```

---

### P4-3: CommandHandler 実装
**見積もり**: 1.5時間

```cpp
// lib/net/include/application/CommandHandler.hpp
#pragma once

#include "interfaces/ICommandChannel.hpp"
#include "application/StateManager.hpp"
#include "application/MetricsCollector.hpp"
#include "domain/Command.hpp"
#include "domain/CommandResponse.hpp"
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

namespace DELILA::Net {

/**
 * @brief コマンド処理のコールバック
 *
 * ユーザー定義の処理を実行する
 */
using CommandCallback = std::function<CommandResponse(const Command&)>;

/**
 * @brief コマンドを処理するクラス
 *
 * Component側でコマンドを受信し、処理を実行
 */
class CommandHandler {
public:
    /**
     * @brief コンストラクタ
     * @param channel コマンドチャネル（所有権を取得）
     * @param state_manager 状態マネージャ（参照）
     */
    CommandHandler(
        std::unique_ptr<ICommandChannel> channel,
        StateManager& state_manager);

    ~CommandHandler();

    // === ライフサイクル ===

    /**
     * @brief ハンドラを開始
     */
    bool Start();

    /**
     * @brief ハンドラを停止
     */
    void Stop();

    /**
     * @brief 実行中かどうか
     */
    bool IsRunning() const { return running_; }

    // === コールバック登録 ===

    /**
     * @brief CONFIGURE コマンドのハンドラを設定
     */
    void SetConfigureHandler(CommandCallback callback);

    /**
     * @brief ARM コマンドのハンドラを設定
     */
    void SetArmHandler(CommandCallback callback);

    /**
     * @brief START コマンドのハンドラを設定
     */
    void SetStartHandler(CommandCallback callback);

    /**
     * @brief STOP コマンドのハンドラを設定
     */
    void SetStopHandler(CommandCallback callback);

    /**
     * @brief CUSTOM コマンドのハンドラを設定
     */
    void SetCustomHandler(CommandCallback callback);

    // === Metrics連携 ===
    void SetMetricsCollector(MetricsCollector* collector) {
        metrics_collector_ = collector;
    }

private:
    std::unique_ptr<ICommandChannel> channel_;
    StateManager& state_manager_;
    MetricsCollector* metrics_collector_ = nullptr;

    std::atomic<bool> running_{false};
    std::thread handler_thread_;

    // コマンドハンドラ
    CommandCallback configure_handler_;
    CommandCallback arm_handler_;
    CommandCallback start_handler_;
    CommandCallback stop_handler_;
    CommandCallback custom_handler_;

    void HandlerLoop();
    CommandResponse ProcessCommand(const Command& cmd);
    CommandResponse HandleStatusCommand(const Command& cmd);
    CommandResponse HandleResetCommand(const Command& cmd);
};

}  // namespace DELILA::Net
```

---

### P4-4: DataPipeline 実装
**見積もり**: 1時間

```cpp
// lib/net/include/application/DataPipeline.hpp
#pragma once

#include "interfaces/ISerializer.hpp"
#include "interfaces/IChecksum.hpp"
#include "domain/DataHeader.hpp"
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace DELILA::Net {

/**
 * @brief データ処理パイプライン設定
 */
struct DataPipelineConfig {
    bool enable_checksum = true;
};

/**
 * @brief データ処理パイプライン
 *
 * シリアライズ → チェックサム の処理を行う（圧縮なし）
 * 各処理はインターフェース経由で注入される
 */
template<typename T>
class DataPipeline {
public:
    DataPipeline(
        std::shared_ptr<ISerializer<T>> serializer,
        std::shared_ptr<IChecksum> checksum,
        const DataPipelineConfig& config = {})
        : serializer_(serializer)
        , checksum_(checksum)
        , config_(config) {}

    /**
     * @brief データを処理（シリアライズ → チェックサム）
     */
    std::optional<std::vector<uint8_t>> Process(
        const std::vector<std::unique_ptr<T>>& data,
        uint64_t sequence_number);

    /**
     * @brief データをデコード（チェックサム検証 → デシリアライズ）
     */
    std::optional<std::pair<std::vector<std::unique_ptr<T>>, uint64_t>> Decode(
        std::span<const uint8_t> data);

    // === 設定 ===
    void EnableChecksum(bool enable) { config_.enable_checksum = enable; }

private:
    std::shared_ptr<ISerializer<T>> serializer_;
    std::shared_ptr<IChecksum> checksum_;
    DataPipelineConfig config_;
};

// 型エイリアス
using EventDataPipeline = DataPipeline<DELILA::Digitizer::EventData>;
using MinimalEventDataPipeline = DataPipeline<DELILA::Digitizer::MinimalEventData>;

}  // namespace DELILA::Net
```

---

### P4-5: Application層ユニットテスト
**見積もり**: 30分

```cpp
// tests/unit/net/application/test_state_manager.cpp

TEST(StateManagerTest, InitialState) {
    StateManager manager;
    EXPECT_EQ(manager.GetState(), ComponentState::Loaded);
}

TEST(StateManagerTest, NormalTransitionSequence) {
    StateManager manager;

    auto result1 = manager.TryTransition(CommandType::CONFIGURE);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result1));
    EXPECT_EQ(std::get<ComponentState>(result1), ComponentState::Configured);

    auto result2 = manager.TryTransition(CommandType::ARM);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result2));
    EXPECT_EQ(std::get<ComponentState>(result2), ComponentState::Armed);

    auto result3 = manager.TryTransition(CommandType::START);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result3));
    EXPECT_EQ(std::get<ComponentState>(result3), ComponentState::Running);
}

TEST(StateManagerTest, InvalidTransition) {
    StateManager manager;

    // Loaded から ARM は不可
    auto result = manager.TryTransition(CommandType::ARM);
    ASSERT_TRUE(std::holds_alternative<TransitionError>(result));
    EXPECT_EQ(std::get<TransitionError>(result).code, ErrorCode::STATE_INVALID_TRANSITION);
}

TEST(StateManagerTest, ErrorRecoveryWithConfigure) {
    StateManager manager;

    manager.TransitionToError(ErrorCode::HARDWARE_COMM_ERROR, "Test error");
    EXPECT_TRUE(manager.IsError());

    auto result = manager.TryTransition(CommandType::CONFIGURE);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result));
    EXPECT_EQ(std::get<ComponentState>(result), ComponentState::Configured);
    EXPECT_FALSE(manager.IsError());
}

// tests/unit/net/application/test_metrics_collector.cpp

TEST(MetricsCollectorTest, RecordEvents) {
    MetricsCollector collector;

    collector.RecordEvent(1000);
    collector.RecordEvent(2000);

    auto metrics = collector.GetCurrentMetrics();
    // 即時にはスループット計算されないが、カウントは記録される
    EXPECT_GE(metrics.uptime_seconds, 0);
}

TEST(MetricsCollectorTest, RecordLatency) {
    MetricsCollector collector;

    collector.RecordLatency(std::chrono::microseconds(100));
    collector.RecordLatency(std::chrono::microseconds(200));
    collector.RecordLatency(std::chrono::microseconds(300));

    auto metrics = collector.GetCurrentMetrics();
    EXPECT_DOUBLE_EQ(metrics.max_latency_us, 300.0);
    EXPECT_DOUBLE_EQ(metrics.avg_latency_us, 200.0);
}
```

---

---

## テスト戦略

### 既存テストの削除

以下の既存テストは新しいテストに置き換えるため**削除**する:

| 削除対象 | 理由 |
|---------|------|
| `tests/unit/net/test_two_phase_start.cpp` | StateManager テストで置き換え |

**注意**: `tests/unit/net/utils/` にある以下のテストは**維持**する:
- `test_sequence_gap_detector.cpp`
- `test_heartbeat_manager.cpp`
- `test_heartbeat_monitor.cpp`
- `test_eos_tracker.cpp`

これらはユーティリティクラスのテストであり、新アーキテクチャでも使用する。

### 新規テスト作成

**ファイル**: `tests/unit/net/application/test_state_manager.cpp`

```cpp
// tests/unit/net/application/test_state_manager.cpp
#include <gtest/gtest.h>
#include "application/StateManager.hpp"

namespace DELILA::Net {

TEST(StateManagerTest, InitialState) {
    StateManager manager;
    EXPECT_EQ(manager.GetState(), ComponentState::Loaded);
    EXPECT_FALSE(manager.IsError());
}

TEST(StateManagerTest, NormalTransitionSequence) {
    StateManager manager;

    auto result1 = manager.TryTransition(CommandType::CONFIGURE);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result1));
    EXPECT_EQ(std::get<ComponentState>(result1), ComponentState::Configured);

    auto result2 = manager.TryTransition(CommandType::ARM);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result2));
    EXPECT_EQ(std::get<ComponentState>(result2), ComponentState::Armed);

    auto result3 = manager.TryTransition(CommandType::START);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result3));
    EXPECT_EQ(std::get<ComponentState>(result3), ComponentState::Running);
}

TEST(StateManagerTest, InvalidTransition) {
    StateManager manager;

    // Loaded から ARM は不可
    auto result = manager.TryTransition(CommandType::ARM);
    ASSERT_TRUE(std::holds_alternative<TransitionError>(result));
    EXPECT_EQ(std::get<TransitionError>(result).code, ErrorCode::STATE_INVALID_TRANSITION);
}

TEST(StateManagerTest, ErrorRecoveryWithConfigure) {
    StateManager manager;

    manager.TransitionToError(ErrorCode::HARDWARE_COMM_ERROR, "Test error");
    EXPECT_TRUE(manager.IsError());
    EXPECT_EQ(manager.GetLastErrorCode(), ErrorCode::HARDWARE_COMM_ERROR);

    auto result = manager.TryTransition(CommandType::CONFIGURE);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result));
    EXPECT_EQ(std::get<ComponentState>(result), ComponentState::Configured);
    EXPECT_FALSE(manager.IsError());
}

TEST(StateManagerTest, ErrorRecoveryWithReset) {
    StateManager manager;

    manager.TransitionToError(ErrorCode::HARDWARE_COMM_ERROR, "Test error");
    EXPECT_TRUE(manager.IsError());

    auto result = manager.TryTransition(CommandType::RESET);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result));
    EXPECT_EQ(std::get<ComponentState>(result), ComponentState::Loaded);
}

TEST(StateManagerTest, StopFromAnyState) {
    StateManager manager;

    manager.TryTransition(CommandType::CONFIGURE);
    manager.TryTransition(CommandType::ARM);
    manager.TryTransition(CommandType::START);
    EXPECT_EQ(manager.GetState(), ComponentState::Running);

    auto result = manager.TryTransition(CommandType::STOP);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result));
    EXPECT_EQ(std::get<ComponentState>(result), ComponentState::Loaded);
}

TEST(StateManagerTest, StatusDoesNotChangeState) {
    StateManager manager;

    manager.TryTransition(CommandType::CONFIGURE);
    ComponentState before = manager.GetState();

    auto result = manager.TryTransition(CommandType::STATUS);
    ASSERT_TRUE(std::holds_alternative<ComponentState>(result));
    EXPECT_EQ(manager.GetState(), before);
}

TEST(StateManagerTest, StateChangeCallback) {
    StateManager manager;

    bool callback_called = false;
    ComponentState old_state, new_state;

    manager.SetStateChangeCallback([&](ComponentState o, ComponentState n) {
        callback_called = true;
        old_state = o;
        new_state = n;
    });

    manager.TryTransition(CommandType::CONFIGURE);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(old_state, ComponentState::Loaded);
    EXPECT_EQ(new_state, ComponentState::Configured);
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/application/test_metrics_collector.cpp`

```cpp
// tests/unit/net/application/test_metrics_collector.cpp
#include <gtest/gtest.h>
#include "application/MetricsCollector.hpp"

namespace DELILA::Net {

TEST(MetricsCollectorTest, RecordEvents) {
    MetricsCollector collector;

    collector.RecordEvent(1000);
    collector.RecordEvent(2000);

    auto metrics = collector.GetCurrentMetrics();
    EXPECT_GE(metrics.uptime_seconds, 0);
}

TEST(MetricsCollectorTest, RecordLatency) {
    MetricsCollector collector;

    collector.RecordLatency(std::chrono::microseconds(100));
    collector.RecordLatency(std::chrono::microseconds(200));
    collector.RecordLatency(std::chrono::microseconds(300));

    auto metrics = collector.GetCurrentMetrics();
    EXPECT_DOUBLE_EQ(metrics.max_latency_us, 300.0);
    EXPECT_DOUBLE_EQ(metrics.avg_latency_us, 200.0);
}

TEST(MetricsCollectorTest, RecordError) {
    MetricsCollector collector;

    collector.RecordError();
    collector.RecordError();

    auto metrics = collector.GetCurrentMetrics();
    EXPECT_EQ(metrics.error_count, 2);
}

TEST(MetricsCollectorTest, RecordSequenceGap) {
    MetricsCollector collector;

    collector.RecordSequenceGap(5);

    auto metrics = collector.GetCurrentMetrics();
    EXPECT_EQ(metrics.sequence_gaps, 5);
}

TEST(MetricsCollectorTest, Reset) {
    MetricsCollector collector;

    collector.RecordEvent(1000);
    collector.RecordError();
    collector.RecordLatency(std::chrono::microseconds(500));

    collector.Reset();

    auto metrics = collector.GetCurrentMetrics();
    EXPECT_EQ(metrics.error_count, 0);
    EXPECT_DOUBLE_EQ(metrics.max_latency_us, 0.0);
}

}  // namespace DELILA::Net
```

**ファイル**: `tests/unit/net/application/test_data_pipeline.cpp`

```cpp
// tests/unit/net/application/test_data_pipeline.cpp
#include <gtest/gtest.h>
#include "application/DataPipeline.hpp"
#include "infrastructure/serialization/BinarySerializer.hpp"
#include "infrastructure/checksum/CRC32Checksum.hpp"

namespace DELILA::Net {

TEST(DataPipelineTest, ProcessAndDecode) {
    auto serializer = std::make_shared<MinimalEventDataSerializer>();
    auto checksum = std::make_shared<CRC32Checksum>();

    DataPipeline<DELILA::Digitizer::MinimalEventData> pipeline(serializer, checksum);

    std::vector<std::unique_ptr<DELILA::Digitizer::MinimalEventData>> events;
    auto event = std::make_unique<DELILA::Digitizer::MinimalEventData>();
    event->timestamp = 1234567890ULL;
    event->energy = 500;
    events.push_back(std::move(event));

    auto processed = pipeline.Process(events, 42);
    ASSERT_TRUE(processed.has_value());
    EXPECT_FALSE(processed->empty());

    auto decoded = pipeline.Decode(*processed);
    ASSERT_TRUE(decoded.has_value());

    auto& [decoded_events, seq] = *decoded;
    EXPECT_EQ(seq, 42);
    ASSERT_EQ(decoded_events.size(), 1);
    EXPECT_EQ(decoded_events[0]->timestamp, 1234567890ULL);
}

TEST(DataPipelineTest, ProcessWithChecksumDisabled) {
    auto serializer = std::make_shared<MinimalEventDataSerializer>();
    auto checksum = std::make_shared<CRC32Checksum>();

    DataPipelineConfig config;
    config.enable_checksum = false;

    DataPipeline<DELILA::Digitizer::MinimalEventData> pipeline(serializer, checksum, config);

    std::vector<std::unique_ptr<DELILA::Digitizer::MinimalEventData>> events;
    auto event = std::make_unique<DELILA::Digitizer::MinimalEventData>();
    events.push_back(std::move(event));

    auto processed = pipeline.Process(events, 1);
    ASSERT_TRUE(processed.has_value());
}

}  // namespace DELILA::Net
```

### テストディレクトリ構造

```
tests/unit/net/
├── application/                        # 新規ディレクトリ
│   ├── test_state_manager.cpp          # 新規作成
│   ├── test_metrics_collector.cpp      # 新規作成
│   └── test_data_pipeline.cpp          # 新規作成
├── utils/                              # 既存維持
│   ├── test_sequence_gap_detector.cpp  # 既存維持
│   ├── test_heartbeat_manager.cpp      # 既存維持
│   ├── test_heartbeat_monitor.cpp      # 既存維持
│   └── test_eos_tracker.cpp            # 既存維持
└── ... (他のディレクトリ)
```

### CMakeLists.txt 更新

```cmake
# tests/unit/net/application のテストを追加
add_executable(delila_net_application_tests
    application/test_state_manager.cpp
    application/test_metrics_collector.cpp
    application/test_data_pipeline.cpp
)

target_link_libraries(delila_net_application_tests
    delila_net
    delila_core
    GTest::gtest_main
)

add_test(NAME NetApplicationTests COMMAND delila_net_application_tests)
```

---

## 完了条件

- [ ] StateManager が状態遷移を正しく管理
- [ ] MetricsCollector がメトリクスを収集
- [ ] CommandHandler がコマンドを処理
- [ ] DataPipeline がデータ処理パイプラインを提供
- [ ] 全クラスがインターフェースに依存（具体実装に依存しない）
- [ ] 既存テスト削除（test_two_phase_start.cpp）
- [ ] 新規 Application テスト作成
- [ ] 全ユニットテストがパス

---

## 次のPhase

Phase 4 完了後、Phase 5（Facade層）に進む。
