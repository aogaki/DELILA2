# Phase 3: Error Handling

**優先度**: 高
**目的**: 体系的なエラーハンドリングとERROR状態からのリカバリ機能を実装
**リカバリ方法**: CONFIGUREコマンドでERROR状態からリセット可能

---

## タスク一覧

### E-1: ErrorCategory/ErrorCode定義
**ファイル**: `lib/net/include/ErrorCode.hpp`, `lib/net/src/ErrorCode.cpp`
**見積もり**: 1時間
**依存**: なし

**注意**: Phase 1のC-3と統合。C-3で基本定義、E-1で追加機能を実装。

#### 実装内容（C-3の拡張）
```cpp
// lib/net/include/ErrorCode.hpp に追加

namespace DELILA::Net {

/**
 * @brief エラー情報を保持する構造体
 */
struct ErrorInfo {
    ErrorCode code = ErrorCode::SUCCESS;
    std::string message;
    std::string source;           // エラー発生元（コンポーネントID等）
    std::chrono::system_clock::time_point timestamp;

    bool IsError() const { return code != ErrorCode::SUCCESS; }
    void Clear();
};

/**
 * @brief エラー履歴を管理するクラス
 */
class ErrorHistory {
public:
    /**
     * @brief エラーを記録
     */
    void RecordError(const ErrorInfo& error);

    /**
     * @brief 最新のエラーを取得
     */
    ErrorInfo GetLastError() const;

    /**
     * @brief エラー履歴を取得
     * @param max_count 最大取得数（0=全て）
     */
    std::vector<ErrorInfo> GetHistory(size_t max_count = 0) const;

    /**
     * @brief 特定カテゴリのエラー数を取得
     */
    size_t GetErrorCount(ErrorCategory category) const;

    /**
     * @brief 履歴をクリア
     */
    void Clear();

private:
    mutable std::mutex mutex_;
    std::deque<ErrorInfo> history_;
    static constexpr size_t MAX_HISTORY_SIZE = 100;
};

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] ErrorInfo::IsError()がSUCCESS以外でtrue
- [ ] ErrorInfo::Clear()でコードがSUCCESSになる
- [ ] ErrorHistory::RecordError()がエラーを記録する
- [ ] ErrorHistory::GetLastError()が最新エラーを返す
- [ ] ErrorHistory::GetHistory()が指定数のエラーを返す
- [ ] ErrorHistory::GetErrorCount()がカテゴリ別にカウントする
- [ ] 履歴がMAX_HISTORY_SIZEを超えると古いものが削除される

#### 完了条件
- [ ] ErrorInfo構造体実装
- [ ] ErrorHistoryクラス実装
- [ ] ユニットテスト作成・パス (`tests/unit/net/test_error_code.cpp` に追加)

---

### E-2: ComponentStateにERROR追加
**ファイル**: `lib/net/include/ComponentState.hpp`
**見積もり**: 30分
**依存**: E-1

#### 実装内容
```cpp
// lib/net/include/ComponentState.hpp を更新

namespace DELILA::Net {

/**
 * @brief コンポーネントの状態
 *
 * 状態遷移図:
 *
 *                    ┌─────────────────────────────────────┐
 *                    │                                     │
 *                    ▼                                     │
 *    ┌────────┐  CONFIGURE  ┌────────────┐  ARM  ┌───────┐│  START  ┌─────────┐
 *    │ Loaded │────────────▶│ Configured │──────▶│ Armed ││────────▶│ Running │
 *    └────────┘             └────────────┘       └───────┘│         └─────────┘
 *        ▲                        ▲                  ▲    │              │
 *        │                        │                  │    │              │
 *        │                   CONFIGURE               │    │              │
 *        │                        │                  │    │              │
 *        │                   ┌────┴────┐             │    │              │
 *        │                   │  Error  │◀────────────┴────┴──────────────┘
 *        │                   └─────────┘        (エラー発生時)
 *        │
 *        └──────────────────────────────────────────────────────────────────
 *                                STOP
 */
enum class ComponentState {
    Loaded,      // 初期状態、未設定
    Configured,  // 設定完了、取得準備可能
    Armed,       // 取得準備完了、開始待ち
    Running,     // データ取得中
    Error        // エラー状態（CONFIGUREでリカバリ可能）
};

// 文字列変換
std::string ComponentStateToString(ComponentState state);
ComponentState StringToComponentState(const std::string& str);

/**
 * @brief 状態遷移が有効かチェック
 * @param from 現在の状態
 * @param to 遷移先の状態
 * @return 遷移が許可されている場合true
 */
bool IsValidTransition(ComponentState from, ComponentState to);

/**
 * @brief コマンドによる状態遷移先を取得
 * @param current 現在の状態
 * @param command コマンドタイプ
 * @return 遷移先の状態（遷移不可の場合はstd::nullopt）
 */
std::optional<ComponentState> GetNextState(ComponentState current, CommandType command);

}  // namespace DELILA::Net
```

#### 状態遷移表

| 現在の状態 | CONFIGURE | ARM | START | STOP | RESET |
|-----------|-----------|-----|-------|------|-------|
| Loaded | Configured | - | - | - | Loaded |
| Configured | Configured | Armed | - | Loaded | Loaded |
| Armed | - | - | Running | Loaded | Loaded |
| Running | - | - | - | Loaded | Loaded |
| **Error** | **Configured** | - | - | - | Loaded |

#### テスト項目
- [ ] ComponentState::Errorが定義されている
- [ ] ComponentStateToString()がErrorを正しく変換
- [ ] StringToComponentState()が"Error"を正しく変換
- [ ] IsValidTransition(Error, Configured)がtrue
- [ ] IsValidTransition(Error, Armed)がfalse
- [ ] IsValidTransition(Error, Running)がfalse
- [ ] GetNextState(Error, CONFIGURE)がConfiguredを返す
- [ ] GetNextState(Error, ARM)がnulloptを返す

#### 完了条件
- [ ] Error状態追加
- [ ] 状態遷移ロジック更新
- [ ] ユニットテスト更新・パス (`tests/unit/net/test_component_state.cpp`)

---

### E-3: 状態遷移ロジック更新 (ERROR対応)
**ファイル**: `lib/net/include/StateManager.hpp`, `lib/net/src/StateManager.cpp`
**見積もり**: 2時間
**依存**: E-2

#### 実装内容
```cpp
// lib/net/include/StateManager.hpp
#pragma once

#include <functional>
#include <mutex>
#include <optional>

#include "ComponentState.hpp"
#include "ErrorCode.hpp"
#include "Command.hpp"

namespace DELILA::Net {

/**
 * @brief コンポーネントの状態を管理するクラス
 *
 * スレッドセーフな状態遷移を提供
 */
class StateManager {
public:
    using StateChangeCallback = std::function<void(ComponentState old_state, ComponentState new_state)>;
    using ErrorCallback = std::function<void(const ErrorInfo& error)>;

    StateManager();
    ~StateManager() = default;

    // === 状態取得 ===

    /**
     * @brief 現在の状態を取得
     */
    ComponentState GetState() const;

    /**
     * @brief 特定の状態かどうかチェック
     */
    bool IsInState(ComponentState state) const;

    /**
     * @brief エラー状態かどうか
     */
    bool IsError() const;

    // === 状態遷移 ===

    /**
     * @brief コマンドに基づいて状態遷移を試行
     * @param command コマンドタイプ
     * @return 成功した場合は新しい状態、失敗した場合はエラーコード
     */
    std::variant<ComponentState, ErrorCode> TryTransition(CommandType command);

    /**
     * @brief エラー状態に遷移
     * @param error エラー情報
     */
    void TransitionToError(const ErrorInfo& error);

    /**
     * @brief 強制的に状態を設定（テスト用）
     */
    void ForceState(ComponentState state);

    // === エラー情報 ===

    /**
     * @brief 最新のエラーを取得
     */
    ErrorInfo GetLastError() const;

    /**
     * @brief エラーをクリア
     */
    void ClearError();

    // === コールバック ===

    /**
     * @brief 状態変更時のコールバックを設定
     */
    void SetStateChangeCallback(StateChangeCallback callback);

    /**
     * @brief エラー発生時のコールバックを設定
     */
    void SetErrorCallback(ErrorCallback callback);

private:
    mutable std::mutex mutex_;
    ComponentState state_ = ComponentState::Loaded;
    ErrorInfo last_error_;

    StateChangeCallback state_change_callback_;
    ErrorCallback error_callback_;

    // 状態遷移を実行（mutex保持下で呼び出し）
    void DoTransition(ComponentState new_state);
};

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] 初期状態がLoadedである
- [ ] TryTransition(CONFIGURE)がConfiguredに遷移
- [ ] TryTransition(ARM)がConfigured→Armedに遷移
- [ ] TryTransition(START)がArmed→Runningに遷移
- [ ] TryTransition(STOP)がLoaded遷移
- [ ] 不正な遷移でエラーコードが返る
- [ ] TransitionToError()でError状態に遷移
- [ ] Error状態からCONFIGUREでConfiguredに遷移
- [ ] Error状態からARMでエラーが返る
- [ ] コールバックが正しく呼ばれる
- [ ] スレッドセーフに動作する

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] 実装ファイル作成
- [ ] ユニットテスト作成・パス (`tests/unit/net/test_state_manager.cpp`)

---

### E-4: CONFIGUREでのERRORリカバリ実装
**ファイル**: `lib/net/src/StateManager.cpp`
**見積もり**: 1時間
**依存**: E-3

#### 実装内容
```cpp
// StateManager.cpp のTryTransition()内

std::variant<ComponentState, ErrorCode> StateManager::TryTransition(CommandType command) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 現在の状態からの遷移先を取得
    auto next_state = GetNextState(state_, command);

    if (!next_state) {
        // Error状態からのCONFIGUREは特別処理
        if (state_ == ComponentState::Error && command == CommandType::CONFIGURE) {
            // エラーをクリアしてConfiguredに遷移
            last_error_.Clear();
            DoTransition(ComponentState::Configured);
            return ComponentState::Configured;
        }

        // 無効な遷移
        return ErrorCode::STATE_INVALID_TRANSITION;
    }

    DoTransition(*next_state);
    return *next_state;
}
```

#### リカバリシナリオテスト
```cpp
TEST(StateManagerTest, RecoverFromErrorWithConfigure) {
    StateManager manager;

    // エラー状態に遷移
    ErrorInfo error;
    error.code = ErrorCode::HARDWARE_COMM_ERROR;
    error.message = "Communication error";
    manager.TransitionToError(error);

    EXPECT_TRUE(manager.IsError());
    EXPECT_EQ(manager.GetLastError().code, ErrorCode::HARDWARE_COMM_ERROR);

    // CONFIGUREでリカバリ
    auto result = manager.TryTransition(CommandType::CONFIGURE);

    ASSERT_TRUE(std::holds_alternative<ComponentState>(result));
    EXPECT_EQ(std::get<ComponentState>(result), ComponentState::Configured);
    EXPECT_FALSE(manager.IsError());
    EXPECT_EQ(manager.GetLastError().code, ErrorCode::SUCCESS);
}

TEST(StateManagerTest, CannotRecoverFromErrorWithArm) {
    StateManager manager;

    manager.TransitionToError({ErrorCode::HARDWARE_COMM_ERROR, "Error"});

    auto result = manager.TryTransition(CommandType::ARM);

    ASSERT_TRUE(std::holds_alternative<ErrorCode>(result));
    EXPECT_EQ(std::get<ErrorCode>(result), ErrorCode::STATE_INVALID_TRANSITION);
    EXPECT_TRUE(manager.IsError());
}
```

#### テスト項目
- [ ] Error状態からCONFIGUREでConfiguredに遷移
- [ ] リカバリ後にエラー情報がクリアされる
- [ ] Error状態からARMがエラーを返す
- [ ] Error状態からSTARTがエラーを返す
- [ ] Error状態からSTOPがエラーを返す
- [ ] Error状態からRESETでLoadedに遷移
- [ ] リカバリ後に正常な状態遷移が可能

#### 完了条件
- [ ] TryTransition()にリカバリロジック追加
- [ ] リカバリシナリオテスト追加・パス

---

### E-5: エラーハンドリングユニットテスト
**ファイル**: `tests/unit/net/test_error_handling.cpp`
**見積もり**: 2時間
**依存**: E-4

#### テストケース一覧

```cpp
// test_error_handling.cpp

// === ErrorInfo テスト ===
TEST(ErrorInfoTest, DefaultIsSuccess)
TEST(ErrorInfoTest, IsErrorReturnsTrueForErrors)
TEST(ErrorInfoTest, ClearResetsToSuccess)

// === ErrorHistory テスト ===
TEST(ErrorHistoryTest, RecordSingleError)
TEST(ErrorHistoryTest, RecordMultipleErrors)
TEST(ErrorHistoryTest, GetLastError)
TEST(ErrorHistoryTest, GetHistoryLimited)
TEST(ErrorHistoryTest, GetHistoryAll)
TEST(ErrorHistoryTest, GetErrorCountByCategory)
TEST(ErrorHistoryTest, ClearHistory)
TEST(ErrorHistoryTest, MaxHistorySizeEnforced)
TEST(ErrorHistoryTest, ConcurrentAccess)

// === StateManager エラーハンドリングテスト ===
TEST(StateManagerErrorTest, TransitionToError)
TEST(StateManagerErrorTest, GetLastError)
TEST(StateManagerErrorTest, ClearError)
TEST(StateManagerErrorTest, ErrorCallbackCalled)
TEST(StateManagerErrorTest, RecoverWithConfigure)
TEST(StateManagerErrorTest, CannotRecoverWithArm)
TEST(StateManagerErrorTest, CannotRecoverWithStart)
TEST(StateManagerErrorTest, RecoverWithReset)
TEST(StateManagerErrorTest, MultipleErrorsAndRecover)

// === エラーシナリオテスト ===
TEST(ErrorScenarioTest, NetworkErrorDuringRunning)
TEST(ErrorScenarioTest, HardwareErrorDuringArm)
TEST(ErrorScenarioTest, ConfigErrorDuringConfigure)
TEST(ErrorScenarioTest, TimeoutError)
TEST(ErrorScenarioTest, ErrorRecoveryAndResume)
```

#### 完了条件
- [ ] 全テストケース実装
- [ ] 全テストパス
- [ ] カバレッジ80%以上

---

## チェックリスト

### Phase 3 完了条件

- [ ] E-1: ErrorCategory/ErrorCode定義 完了（C-3と統合）
- [ ] E-2: ComponentStateにERROR追加 完了
- [ ] E-3: 状態遷移ロジック更新 (ERROR対応) 完了
- [ ] E-4: CONFIGUREでのERRORリカバリ実装 完了
- [ ] E-5: エラーハンドリングユニットテスト 完了
- [ ] 全テスト合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

---

## 実装順序

```
E-1 ──→ E-2 ──→ E-3 ──→ E-4 ──→ E-5
```

**注意**: E-1はPhase 1のC-3と統合して実装可能
