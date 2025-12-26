# Phase 4: IComponent インターフェース

**優先度**: 中
**目的**: コンポーネント共通インターフェースを定義し、Source/Sinkの実装基盤を整備
**アプローチ**: 抽象基底クラス不要、インターフェースのみ定義

---

## タスク一覧

### I-1: IComponentインターフェース定義
**ファイル**: `lib/net/include/IComponent.hpp`
**見積もり**: 1時間
**依存**: Phase 1-3 完了

#### 実装内容
```cpp
// lib/net/include/IComponent.hpp
#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "Command.hpp"
#include "ComponentState.hpp"
#include "ErrorCode.hpp"
#include "Metrics.hpp"

namespace DELILA::Net {

/**
 * @brief コンポーネントインターフェース
 *
 * Source、Sink、その他のコンポーネントが実装すべき共通インターフェース。
 * ライフサイクル管理、状態管理、メトリクス取得、コマンド処理を定義。
 */
class IComponent {
public:
    virtual ~IComponent() = default;

    // ==========================================================================
    // ライフサイクル管理
    // ==========================================================================

    /**
     * @brief コンポーネントを初期化
     * @param config 設定パラメータ（JSON形式）
     * @return 成功/失敗
     *
     * @pre 状態: Loaded
     * @post 成功時: 状態変化なし、失敗時: Error
     *
     * 設定例:
     * {
     *     "component_id": "source_01",
     *     "group_id": "sources",
     *     "network": {
     *         "data_address": "tcp://localhost:5555",
     *         "command_address": "tcp://localhost:5557"
     *     },
     *     "hardware": { ... }
     * }
     */
    virtual bool Initialize(const nlohmann::json& config) = 0;

    /**
     * @brief コンポーネントを設定
     * @return 成功/失敗
     *
     * @pre 状態: Loaded または Error
     * @post 成功時: Configured、失敗時: Error
     */
    virtual bool Configure() = 0;

    /**
     * @brief 取得準備（Two-Phase Start Phase 1）
     * @return 成功/失敗
     *
     * @pre 状態: Configured
     * @post 成功時: Armed、失敗時: Error
     */
    virtual bool Arm() = 0;

    /**
     * @brief 取得開始（Two-Phase Start Phase 2）
     * @return 成功/失敗
     *
     * @pre 状態: Armed
     * @post 成功時: Running、失敗時: Error
     */
    virtual bool Start() = 0;

    /**
     * @brief 取得停止
     * @return 成功/失敗
     *
     * @pre 状態: Running または Armed
     * @post 成功時: Loaded、失敗時: Error
     */
    virtual bool Stop() = 0;

    /**
     * @brief リセット（任意の状態からLoadedに戻す）
     * @return 成功/失敗
     *
     * @pre 状態: Any
     * @post 成功時: Loaded、失敗時: Error
     */
    virtual bool Reset() = 0;

    // ==========================================================================
    // 状態管理
    // ==========================================================================

    /**
     * @brief 現在の状態を取得
     * @return 現在のComponentState
     */
    virtual ComponentState GetState() const = 0;

    /**
     * @brief コンポーネントIDを取得
     * @return コンポーネントの一意識別子
     */
    virtual std::string GetComponentId() const = 0;

    /**
     * @brief グループIDを取得
     * @return 所属グループの識別子
     */
    virtual std::string GetGroupId() const = 0;

    /**
     * @brief エラー状態かどうかを確認
     * @return Error状態の場合true
     */
    virtual bool IsError() const = 0;

    /**
     * @brief 最新のエラー情報を取得
     * @return エラー情報
     */
    virtual ErrorInfo GetLastError() const = 0;

    // ==========================================================================
    // メトリクス
    // ==========================================================================

    /**
     * @brief 現在のメトリクスを取得
     * @return パフォーマンスメトリクス
     */
    virtual Metrics GetMetrics() const = 0;

    /**
     * @brief 完全なステータス情報を取得
     * @return ComponentStatus（状態、メトリクス、エラー情報を含む）
     */
    virtual ComponentStatus GetStatus() const = 0;

    // ==========================================================================
    // コマンド処理
    // ==========================================================================

    /**
     * @brief コマンドを処理
     * @param cmd 受信したコマンド
     * @return コマンドの処理結果
     *
     * 標準コマンド（CONFIGURE, ARM, START, STOP, STATUS, RESET）は
     * デフォルトで適切に処理される。
     * CUSTOMコマンドは派生クラスでオーバーライドして処理。
     */
    virtual CommandResponse HandleCommand(const Command& cmd) = 0;
};

/**
 * @brief コンポーネントファクトリ関数の型
 */
using ComponentFactory = std::function<std::unique_ptr<IComponent>(const nlohmann::json& config)>;

}  // namespace DELILA::Net
```

#### テスト項目
- [ ] インターフェースが正しくコンパイルできる
- [ ] 純粋仮想関数が全て宣言されている
- [ ] ComponentFactoryが正しく定義されている

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] コンパイル確認

---

### I-2: コマンドハンドラ基本実装
**ファイル**: `lib/net/include/ComponentCommandHandler.hpp`, `lib/net/src/ComponentCommandHandler.cpp`
**見積もり**: 2時間
**依存**: I-1

#### 実装内容
```cpp
// lib/net/include/ComponentCommandHandler.hpp
#pragma once

#include "Command.hpp"
#include "IComponent.hpp"

namespace DELILA::Net {

/**
 * @brief コンポーネント用コマンドハンドラヘルパー
 *
 * IComponentのHandleCommand()実装を支援するユーティリティクラス。
 * 標準コマンドの処理ロジックを提供。
 */
class ComponentCommandHandler {
public:
    /**
     * @brief コンストラクタ
     * @param component コマンドを処理するコンポーネント
     */
    explicit ComponentCommandHandler(IComponent* component);

    /**
     * @brief コマンドを処理
     * @param cmd 受信したコマンド
     * @return 処理結果
     *
     * 標準コマンドは内部で処理し、CUSTOMコマンドはfalseを返す
     */
    CommandResponse HandleCommand(const Command& cmd);

    /**
     * @brief カスタムコマンドハンドラを登録
     * @param command_type カスタムコマンドタイプ文字列
     * @param handler ハンドラ関数
     */
    using CustomHandler = std::function<CommandResponse(const Command&)>;
    void RegisterCustomHandler(const std::string& command_type, CustomHandler handler);

private:
    IComponent* component_;
    std::map<std::string, CustomHandler> custom_handlers_;

    // 標準コマンドの処理
    CommandResponse HandleConfigure(const Command& cmd);
    CommandResponse HandleArm(const Command& cmd);
    CommandResponse HandleStart(const Command& cmd);
    CommandResponse HandleStop(const Command& cmd);
    CommandResponse HandleStatus(const Command& cmd);
    CommandResponse HandleReset(const Command& cmd);
    CommandResponse HandleCustom(const Command& cmd);

    // レスポンス作成ヘルパー
    CommandResponse MakeSuccessResponse(const Command& cmd);
    CommandResponse MakeErrorResponse(const Command& cmd, ErrorCode code, const std::string& message);
};

}  // namespace DELILA::Net
```

#### 実装詳細
```cpp
// ComponentCommandHandler.cpp

CommandResponse ComponentCommandHandler::HandleCommand(const Command& cmd) {
    // ターゲット確認
    if (!cmd.target_id.empty() &&
        cmd.target_id != component_->GetComponentId() &&
        cmd.target_id != component_->GetGroupId()) {
        // このコンポーネント宛ではない
        return MakeErrorResponse(cmd, ErrorCode::CONFIG_INVALID, "Command not for this component");
    }

    switch (cmd.command_type) {
        case CommandType::CONFIGURE:
            return HandleConfigure(cmd);
        case CommandType::ARM:
            return HandleArm(cmd);
        case CommandType::START:
            return HandleStart(cmd);
        case CommandType::STOP:
            return HandleStop(cmd);
        case CommandType::STATUS:
            return HandleStatus(cmd);
        case CommandType::RESET:
            return HandleReset(cmd);
        case CommandType::CUSTOM:
            return HandleCustom(cmd);
        default:
            return MakeErrorResponse(cmd, ErrorCode::INTERNAL_NOT_IMPLEMENTED, "Unknown command type");
    }
}

CommandResponse ComponentCommandHandler::HandleConfigure(const Command& cmd) {
    if (component_->Configure()) {
        return MakeSuccessResponse(cmd);
    } else {
        return MakeErrorResponse(cmd, component_->GetLastError().code,
                                 component_->GetLastError().message);
    }
}

CommandResponse ComponentCommandHandler::HandleStatus(const Command& cmd) {
    // STATUSコマンドは常に成功し、現在の状態を返す
    auto response = MakeSuccessResponse(cmd);
    // メトリクスは応答には含まれない（ComponentStatusで別途取得）
    return response;
}

CommandResponse ComponentCommandHandler::HandleCustom(const Command& cmd) {
    auto it = custom_handlers_.find(cmd.command_type_str);
    if (it != custom_handlers_.end()) {
        return it->second(cmd);
    }
    return MakeErrorResponse(cmd, ErrorCode::INTERNAL_NOT_IMPLEMENTED,
                             "Custom command not implemented: " + cmd.command_type_str);
}
```

#### テスト項目
- [ ] CONFIGUREコマンドが正しく処理される
- [ ] ARMコマンドが正しく処理される
- [ ] STARTコマンドが正しく処理される
- [ ] STOPコマンドが正しく処理される
- [ ] STATUSコマンドが正しく処理される
- [ ] RESETコマンドが正しく処理される
- [ ] CUSTOMコマンドが登録ハンドラに委譲される
- [ ] 未登録のCUSTOMコマンドでエラーが返る
- [ ] ターゲットIDが一致しない場合にエラーが返る
- [ ] グループIDでもコマンドが処理される
- [ ] 失敗時に適切なエラーコードが返る
- [ ] request_idが正しく引き継がれる

#### 完了条件
- [ ] ヘッダーファイル作成
- [ ] 実装ファイル作成
- [ ] ユニットテスト作成・パス

---

### I-3: IComponentテスト（モック使用）
**ファイル**: `tests/unit/net/test_icomponent.cpp`
**見積もり**: 2時間
**依存**: I-2

#### 実装内容
```cpp
// tests/unit/net/test_icomponent.cpp

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ComponentCommandHandler.hpp"
#include "IComponent.hpp"

using namespace DELILA::Net;
using ::testing::Return;

/**
 * @brief IComponentのモック実装
 */
class MockComponent : public IComponent {
public:
    MOCK_METHOD(bool, Initialize, (const nlohmann::json& config), (override));
    MOCK_METHOD(bool, Configure, (), (override));
    MOCK_METHOD(bool, Arm, (), (override));
    MOCK_METHOD(bool, Start, (), (override));
    MOCK_METHOD(bool, Stop, (), (override));
    MOCK_METHOD(bool, Reset, (), (override));

    MOCK_METHOD(ComponentState, GetState, (), (const, override));
    MOCK_METHOD(std::string, GetComponentId, (), (const, override));
    MOCK_METHOD(std::string, GetGroupId, (), (const, override));
    MOCK_METHOD(bool, IsError, (), (const, override));
    MOCK_METHOD(ErrorInfo, GetLastError, (), (const, override));

    MOCK_METHOD(Metrics, GetMetrics, (), (const, override));
    MOCK_METHOD(ComponentStatus, GetStatus, (), (const, override));

    MOCK_METHOD(CommandResponse, HandleCommand, (const Command& cmd), (override));
};

class ComponentCommandHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_component_ = std::make_unique<MockComponent>();

        // デフォルトの戻り値を設定
        ON_CALL(*mock_component_, GetComponentId())
            .WillByDefault(Return("test_component"));
        ON_CALL(*mock_component_, GetGroupId())
            .WillByDefault(Return("test_group"));
        ON_CALL(*mock_component_, GetState())
            .WillByDefault(Return(ComponentState::Loaded));
        ON_CALL(*mock_component_, IsError())
            .WillByDefault(Return(false));
        ON_CALL(*mock_component_, GetLastError())
            .WillByDefault(Return(ErrorInfo{}));

        handler_ = std::make_unique<ComponentCommandHandler>(mock_component_.get());
    }

    std::unique_ptr<MockComponent> mock_component_;
    std::unique_ptr<ComponentCommandHandler> handler_;
};

// === 基本コマンドテスト ===

TEST_F(ComponentCommandHandlerTest, HandleConfigureSuccess) {
    EXPECT_CALL(*mock_component_, Configure())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_component_, GetState())
        .WillOnce(Return(ComponentState::Configured));

    Command cmd;
    cmd.command_type = CommandType::CONFIGURE;
    cmd.target_id = "test_component";
    cmd.request_id = 123;

    auto response = handler_->HandleCommand(cmd);

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.request_id, 123);
    EXPECT_EQ(response.current_state, ComponentState::Configured);
}

TEST_F(ComponentCommandHandlerTest, HandleConfigureFailure) {
    ErrorInfo error;
    error.code = ErrorCode::HARDWARE_INIT_FAILED;
    error.message = "Hardware initialization failed";

    EXPECT_CALL(*mock_component_, Configure())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_component_, GetLastError())
        .WillOnce(Return(error));
    EXPECT_CALL(*mock_component_, GetState())
        .WillOnce(Return(ComponentState::Error));

    Command cmd;
    cmd.command_type = CommandType::CONFIGURE;
    cmd.target_id = "test_component";

    auto response = handler_->HandleCommand(cmd);

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error_code, ErrorCode::HARDWARE_INIT_FAILED);
    EXPECT_EQ(response.error_message, "Hardware initialization failed");
}

TEST_F(ComponentCommandHandlerTest, HandleArmSuccess) {
    EXPECT_CALL(*mock_component_, Arm())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_component_, GetState())
        .WillOnce(Return(ComponentState::Armed));

    Command cmd;
    cmd.command_type = CommandType::ARM;
    cmd.target_id = "test_component";

    auto response = handler_->HandleCommand(cmd);

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.current_state, ComponentState::Armed);
}

TEST_F(ComponentCommandHandlerTest, HandleStatusAlwaysSucceeds) {
    EXPECT_CALL(*mock_component_, GetState())
        .WillOnce(Return(ComponentState::Running));

    Command cmd;
    cmd.command_type = CommandType::STATUS;
    cmd.target_id = "test_component";

    auto response = handler_->HandleCommand(cmd);

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.current_state, ComponentState::Running);
}

// === ターゲット指定テスト ===

TEST_F(ComponentCommandHandlerTest, IgnoreCommandForOtherComponent) {
    Command cmd;
    cmd.command_type = CommandType::CONFIGURE;
    cmd.target_id = "other_component";  // 別のコンポーネント宛

    auto response = handler_->HandleCommand(cmd);

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error_code, ErrorCode::CONFIG_INVALID);
}

TEST_F(ComponentCommandHandlerTest, AcceptCommandForGroup) {
    EXPECT_CALL(*mock_component_, Configure())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_component_, GetState())
        .WillOnce(Return(ComponentState::Configured));

    Command cmd;
    cmd.command_type = CommandType::CONFIGURE;
    cmd.target_id = "test_group";  // グループID宛

    auto response = handler_->HandleCommand(cmd);

    EXPECT_TRUE(response.success);
}

// === カスタムコマンドテスト ===

TEST_F(ComponentCommandHandlerTest, HandleRegisteredCustomCommand) {
    handler_->RegisterCustomHandler("CALIBRATE", [](const Command& cmd) {
        CommandResponse response;
        response.request_id = cmd.request_id;
        response.success = true;
        response.current_state = ComponentState::Configured;
        return response;
    });

    Command cmd;
    cmd.command_type = CommandType::CUSTOM;
    cmd.command_type_str = "CALIBRATE";
    cmd.target_id = "test_component";
    cmd.request_id = 456;

    auto response = handler_->HandleCommand(cmd);

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.request_id, 456);
}

TEST_F(ComponentCommandHandlerTest, HandleUnregisteredCustomCommand) {
    Command cmd;
    cmd.command_type = CommandType::CUSTOM;
    cmd.command_type_str = "UNKNOWN_CUSTOM";
    cmd.target_id = "test_component";

    auto response = handler_->HandleCommand(cmd);

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error_code, ErrorCode::INTERNAL_NOT_IMPLEMENTED);
}
```

#### 完了条件
- [ ] MockComponent実装
- [ ] 全テストケース実装
- [ ] 全テストパス

---

## チェックリスト

### Phase 4 完了条件

- [ ] I-1: IComponentインターフェース定義 完了
- [ ] I-2: コマンドハンドラ基本実装 完了
- [ ] I-3: IComponentテスト（モック使用） 完了
- [ ] 全テスト合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

---

## 実装順序

```
I-1 ──→ I-2 ──→ I-3
```

---

## 今後の拡張（参考）

Phase 4完了後、実際のコンポーネント実装に進む際の参考：

### SourceComponent の概要

```cpp
class SourceComponent : public IComponent {
public:
    // IComponent実装
    // ...

    // Source固有メソッド
    void SetDataCallback(std::function<void(EventDataBatch)> callback);

private:
    std::unique_ptr<Digitizer> digitizer_;
    std::unique_ptr<ZMQTransport> transport_;
    std::unique_ptr<MetricsCollector> metrics_;
    std::unique_ptr<StateManager> state_manager_;
    ComponentCommandHandler command_handler_;
};
```

### SinkComponent の概要

```cpp
class SinkComponent : public IComponent {
public:
    // IComponent実装
    // ...

    // Sink固有メソッド
    void SetDataHandler(std::function<void(EventDataBatch)> handler);

private:
    std::unique_ptr<ZMQTransport> transport_;
    std::unique_ptr<MetricsCollector> metrics_;
    std::unique_ptr<StateManager> state_manager_;
    ComponentCommandHandler command_handler_;
};
```
