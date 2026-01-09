# テスト戦略

## 基本原則

### TDD (Test-Driven Development)

```
1. Red   - 失敗するテストを書く
2. Green - テストを通す最小限のコードを書く
3. Refactor - コードを整理（テストは常にGreen）
```

**テストなしのコード追加は禁止**

### テストの優先順位

1. ユニットテスト（必須）
2. 統合テスト（必須）
3. ベンチマーク（オプション）

---

## 1. テストフレームワーク

| 用途 | フレームワーク |
|------|----------------|
| ユニットテスト | Google Test |
| モック | Google Mock |
| ベンチマーク | Google Benchmark |

---

## 2. テスト構成

```
tests/
├── unit/
│   ├── core/
│   │   ├── test_component_state.cpp
│   │   ├── test_command.cpp
│   │   └── test_error_code.cpp
│   ├── component/
│   │   ├── test_digitizer_source.cpp
│   │   ├── test_file_writer.cpp
│   │   ├── test_time_sort_merger.cpp
│   │   └── test_cli_operator.cpp
│   └── net/
│       ├── test_zmq_transport.cpp
│       ├── test_data_processor.cpp
│       └── test_two_phase_start.cpp
├── integration/
│   ├── test_source_to_writer.cpp
│   ├── test_full_pipeline.cpp
│   └── test_graceful_stop.cpp
└── benchmarks/
    ├── bench_serialization.cpp
    └── bench_throughput.cpp
```

---

## 3. ユニットテスト

### 3.1 状態遷移テスト

全コンポーネント共通のテストパターン。

```cpp
// 正常な状態遷移
TEST(ComponentStateTest, ValidTransitions) {
    // Idle -> Configuring -> Configured
    EXPECT_TRUE(IsValidTransition(ComponentState::Idle, ComponentState::Configuring));
    EXPECT_TRUE(IsValidTransition(ComponentState::Configuring, ComponentState::Configured));
    // ... 全パターン
}

// 無効な状態遷移
TEST(ComponentStateTest, InvalidTransitions) {
    // Idle -> Running は不可
    EXPECT_FALSE(IsValidTransition(ComponentState::Idle, ComponentState::Running));
}
```

### 3.2 コンポーネントテスト

各コンポーネントで必須のテスト項目。

```cpp
class DigitizerSourceTest : public ::testing::Test {
protected:
    void SetUp() override {
        source_ = std::make_unique<DigitizerSource>();
    }
    std::unique_ptr<DigitizerSource> source_;
};

// 初期状態
TEST_F(DigitizerSourceTest, InitialState) {
    EXPECT_EQ(source_->GetState(), ComponentState::Idle);
}

// 設定
TEST_F(DigitizerSourceTest, Configure) {
    EXPECT_TRUE(source_->Initialize("test_config.json"));
    EXPECT_EQ(source_->GetState(), ComponentState::Configured);
}

// Arm/Start/Stop
TEST_F(DigitizerSourceTest, FullLifecycle) {
    source_->Initialize("test_config.json");
    // Arm -> Armed -> Start -> Running -> Stop -> Configured
}

// エラー処理
TEST_F(DigitizerSourceTest, InvalidConfigReturnsError) {
    EXPECT_FALSE(source_->Initialize("nonexistent.json"));
    EXPECT_EQ(source_->GetState(), ComponentState::Error);
}
```

### 3.3 モックの使用

外部依存はモックで置き換え。

```cpp
class MockDigitizer : public IDigitizer {
public:
    MOCK_METHOD(bool, Initialize, (), (override));
    MOCK_METHOD(bool, StartAcquisition, (), (override));
    MOCK_METHOD(bool, StopAcquisition, (), (override));
    MOCK_METHOD(std::vector<EventData>, ReadData, (), (override));
};

TEST_F(DigitizerSourceTest, UsesDigitizerInterface) {
    auto mockDigitizer = std::make_unique<MockDigitizer>();
    EXPECT_CALL(*mockDigitizer, Initialize()).WillOnce(Return(true));

    DigitizerSource source(std::move(mockDigitizer));
    source.Initialize("config.json");
}
```

---

## 4. 統合テスト

### 4.1 パイプラインテスト

```cpp
TEST(IntegrationTest, SourceToWriter) {
    // Setup
    DigitizerSource source;
    FileWriter writer;

    source.Initialize("source_config.json");
    writer.Initialize("writer_config.json");

    // Connect
    source.SetOutputAddresses({"tcp://localhost:5555"});
    writer.SetInputAddresses({"tcp://localhost:5555"});

    // Run
    source.OnArm();
    writer.OnArm();
    source.OnStart(1);
    writer.OnStart(1);

    // Wait for data
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop
    source.OnStop(true);
    writer.OnStop(true);

    // Verify
    EXPECT_GT(writer.GetStatus().metrics.events_processed, 0);
}
```

### 4.2 GracefulStopテスト

```cpp
TEST(IntegrationTest, GracefulStopFlushesData) {
    // Source -> Writer パイプライン
    // データ送信中にGracefulStop
    // 全データがファイルに書き込まれていることを確認
}
```

### 4.3 エラーリカバリテスト

```cpp
TEST(IntegrationTest, RecoverFromError) {
    // エラー状態からResetで復帰
    // 再度Configure/Arm/Startできることを確認
}
```

---

## 5. テスト実行

### コマンド

```bash
# 全テスト実行
cd build && ctest --output-on-failure

# 特定テスト実行
ctest -R DigitizerSourceTest --output-on-failure

# 詳細出力
ctest -V

# 並列実行
ctest -j$(nproc)
```

### CI統合

```yaml
# GitHub Actions例
- name: Run Tests
  run: |
    cd build
    ctest --output-on-failure -j4
```

---

## 6. カバレッジ

### 目標

| 対象 | カバレッジ目標 |
|------|----------------|
| lib/core | >= 90% |
| lib/component | >= 80% |
| lib/net | >= 80% |

### 測定

```bash
# カバレッジ付きビルド
cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..
make
ctest
make coverage
```

---

## 7. テスト作成チェックリスト

新しいクラスを実装する際は、以下のテストを作成:

- [ ] 初期状態テスト
- [ ] 正常系テスト（全メソッド）
- [ ] 異常系テスト（無効な入力、エラー状態）
- [ ] 状態遷移テスト
- [ ] 境界値テスト

---

## 8. 関連ドキュメント

- [00_OVERVIEW.md](00_OVERVIEW.md) - 全体概要
- [01_REQUIREMENTS.md](01_REQUIREMENTS.md) - 要求仕様
- [02_INTERFACE_DESIGN.md](02_INTERFACE_DESIGN.md) - インターフェース設計
- [03_IMPLEMENTATION_PLAN.md](03_IMPLEMENTATION_PLAN.md) - 実装計画
- [CLAUDE.md](../CLAUDE.md) - 開発ガイドライン（TDDセクション）
