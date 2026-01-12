# 実装計画

## 基本原則

### KISS (Keep It Simple, Stupid)

- 最小限の機能から開始し、動くものを優先
- 過度な抽象化・汎用化を避ける
- 「必要になったら追加」の姿勢を徹底
- 1つの関数は1つの責務のみ

### TDD (Test-Driven Development)

- **Red**: 失敗するテストを先に書く
- **Green**: テストを通す最小限のコードを書く
- **Refactor**: コードを整理（テストは常にGreen）

```
テストなしのコード追加は禁止
```

---

## 1. 現状

### 実装済み（lib/core）

| ファイル | 内容 | 状態 |
|----------|------|------|
| ComponentState.hpp | 状態enum、遷移チェック | ✅ 完了 |
| IComponent.hpp | 基底インターフェース | ✅ 完了 |
| IDataComponent.hpp | データコンポーネントIF | ✅ Phase 1完了 |
| IOperator.hpp | オペレーターIF | ✅ Phase 1完了 |
| ComponentStatus.hpp | ステータス構造体 | ✅ 完了 |
| ComponentConfig.hpp | コンポーネント設定 | ✅ Phase 1完了 |
| OperatorConfig.hpp | オペレーター設定 | ✅ Phase 1完了 |
| Command.hpp | コマンド構造体 | ✅ 完了 |
| CommandResponse.hpp | レスポンス構造体 | ✅ 完了 |
| ErrorCode.hpp | エラーコードenum | ✅ 完了 |

### 実装済み（lib/net）

| ファイル | 内容 | 状態 |
|----------|------|------|
| ZMQTransport.hpp | ZeroMQ通信 | ✅ 完了 |
| DataProcessor.hpp | シリアライズ/デシリアライズ | ✅ 完了 |
| TwoPhaseStartManager.hpp | 状態遷移管理 | ✅ 完了 |
| HeartbeatManager.hpp | ハートビート送信 | ✅ 完了 |
| HeartbeatMonitor.hpp | ハートビート監視 | ✅ 完了 |
| EOSTracker.hpp | EOS追跡 | ✅ 完了 |

### 実装済み（lib/component）- Phase 4完了

| ファイル | 内容 | 状態 |
|----------|------|------|
| DigitizerSource.hpp/cpp | データソース（モック対応） | ✅ Phase 2完了 |
| FileWriter.hpp/cpp | ファイル書き込み | ✅ Phase 2完了 |
| TimeSortMerger.hpp/cpp | 時間ソートマージャー | ✅ Phase 3完了 |
| CLIOperator.hpp/cpp | CLIオペレーター | ✅ Phase 4完了 |

### 未実装

なし（全Phase完了）

---

## 2. 実装フェーズ

### Phase 1: インターフェース完成 ✅ 完了

**目標**: 全インターフェースを定義し、モックでテスト可能にする

**完了項目**:
- [x] IDataComponent.hpp - データコンポーネントインターフェース
- [x] IOperator.hpp - オペレーターインターフェース（JobState, JobStatus含む）
- [x] ComponentConfig.hpp - コンポーネント設定（TransportConfig含む）
- [x] OperatorConfig.hpp - オペレーター設定

**テスト結果**:
- test_idatacomponent.cpp - 全テスト通過
- test_ioperator.cpp - 16テスト通過
- test_component_config.cpp - 11テスト通過
- test_operator_config.cpp - 14テスト通過

### Phase 2: 最小構成の動作確認 ✅ 完了

**目標**: DigitizerSource → FileWriter の最小パイプライン

**完了項目**:
- [x] DigitizerSource（モックモード対応）
  - 状態遷移（Idle→Configured→Armed→Running→Configured）
  - モックイベント生成
  - ZMQ PUSH送信
- [x] FileWriter
  - 状態遷移
  - ZMQ PULL受信
  - バイナリファイル書き込み
- [x] Source→Writer統合テスト

**テスト結果**:
- test_digitizer_source.cpp - 19テスト通過
- test_file_writer.cpp - 21テスト通過
- test_source_writer_pipeline.cpp - 15テスト通過（統合テスト）

**実装ファイル**:
```
lib/component/
├── include/
│   ├── DigitizerSource.hpp
│   └── FileWriter.hpp
└── src/
    ├── DigitizerSource.cpp
    └── FileWriter.cpp
```

### Phase 3: マージャー追加 ✅ 完了

**目標**: 複数Source → TimeSortMerger → FileWriter

**完了項目**:
- [x] TimeSortMerger
  - 複数入力からのデータ受信（N個のPULLソケット）
  - 設定可能なソートウィンドウ（デフォルト10ms）
  - マージ済みデータの出力（PUSHソケット）
  - EOSTracker連携
- [x] 複数Source→Merger→Writer 統合テスト

**テスト結果**:
- test_time_sort_merger.cpp - 23テスト通過（ユニットテスト）
- test_multi_source_merger_pipeline.cpp - 14テスト通過（統合テスト）

**実装ファイル**:
```
lib/component/
├── include/
│   ├── DigitizerSource.hpp
│   ├── FileWriter.hpp
│   └── TimeSortMerger.hpp
└── src/
    ├── DigitizerSource.cpp
    ├── FileWriter.cpp
    └── TimeSortMerger.cpp
```

**注意**: 時間ベースのソート・マージロジックはTODOとして残っている。
現在は複数入力からの受信と基本的なパイプライン動作が確認済み。

### Phase 4: オペレーター ✅ 完了

**目標**: CLIからシステム全体を制御

**完了項目**:
- [x] CLIOperator
  - IOperatorインターフェース実装
  - 非同期ジョブ実行（ConfigureAllAsync, ArmAllAsync, StartAllAsync, StopAllAsync, ResetAllAsync）
  - ジョブステータス追跡（Pending, Running, Completed, Failed）
  - コンポーネント登録/解除
  - コンポーネントステータス監視

**テスト結果**:
- test_cli_operator.cpp - 36テスト通過（ユニットテスト）

**実装ファイル**:
```
lib/component/
├── include/
│   ├── DigitizerSource.hpp
│   ├── FileWriter.hpp
│   ├── TimeSortMerger.hpp
│   └── CLIOperator.hpp     ✅ Phase 4完了
└── src/
    ├── DigitizerSource.cpp
    ├── FileWriter.cpp
    ├── TimeSortMerger.cpp
    └── CLIOperator.cpp      ✅ Phase 4完了
```

**追加テストユーティリティ**:
- tests/integration/test_utils.hpp - イベントベースの待機ユーティリティ
  - `WaitForCondition()` - 条件が満たされるまでポーリング
  - `WaitForArmed()` - Armed状態を待機
  - `WaitForRunning()` - Running状態を待機
  - `WaitForConfigured()` - Configured状態を待機
  - `WaitForSocketSetup()` - ZMQソケット確立待機

### Phase 5: 統合テスト ✅ 完了

**目標**: CLIOperatorとDataComponents連携の結合動作確認

**完了項目**:
- [x] CLIOperator → DataComponents ZMQコマンド連携
  - 単一コンポーネント制御（Configure, Arm, Start, Stop, Reset）
  - 複数コンポーネント同時制御
- [x] 全コンポーネント統合パイプライン
  - Source → Writer パイプライン
  - Source → Merger → Writer パイプライン
- [x] GracefulStop動作確認
- [x] エラーリカバリ動作確認

**テスト結果**:
- test_operator_component_integration.cpp - 7テスト通過（統合テスト）
  - SingleComponentControlTest: 5テスト
  - MultipleComponentControlTest: 2テスト

---

## 3. 依存関係

```
Phase 1 ─────────────────────────────────────────────┐
    IDataComponent.hpp                               │
    IOperator.hpp                                    │
    ComponentConfig.hpp                              │
    OperatorConfig.hpp                               │
                                                     ▼
Phase 2 ─────────────────────────────────────────────┐
    DigitizerSource (IDataComponent実装)             │
    FileWriter (IDataComponent実装)                  │
        └─ 使用: ZMQTransport, DataProcessor         │
                                                     ▼
Phase 3 ─────────────────────────────────────────────┐
    TimeSortMerger (IDataComponent実装)              │
        └─ 使用: EOSTracker                          │
                                                     ▼
Phase 4 ─────────────────────────────────────────────┐
    CLIOperator (IOperator実装)                      │
        └─ 使用: HeartbeatMonitor                    │
                                                     ▼
Phase 5 ✅ 完了
    統合テスト（Operator + DataComponents連携）
```

---

## 4. 既存コードの活用

### lib/net から再利用

| クラス | 用途 |
|--------|------|
| ZMQTransport | 全コンポーネントのネットワーク通信 |
| DataProcessor | シリアライズ/デシリアライズ |
| TwoPhaseStartManager | 状態遷移管理（参考実装として） |
| HeartbeatManager | ステータス定期送信 |
| HeartbeatMonitor | Operatorでの監視 |
| EOSTracker | Merger/Writerでの停止検知 |

### lib/digitizer から再利用

| クラス | 用途 |
|--------|------|
| IDigitizer | DigitizerSourceで使用 |

---

## 5. ファイル配置

```
lib/core/include/delila/core/
├── ComponentState.hpp      ✅ 完了
├── IComponent.hpp          ✅ 完了
├── IDataComponent.hpp      ✅ Phase 1完了
├── IOperator.hpp           ✅ Phase 1完了
├── ComponentStatus.hpp     ✅ 完了
├── Command.hpp             ✅ 完了
├── CommandResponse.hpp     ✅ 完了
├── ErrorCode.hpp           ✅ 完了
├── ComponentConfig.hpp     ✅ Phase 1完了
└── OperatorConfig.hpp      ✅ Phase 1完了

lib/component/
├── CMakeLists.txt          ✅ Phase 4完了
├── include/
│   ├── DigitizerSource.hpp  ✅ Phase 2完了
│   ├── FileWriter.hpp       ✅ Phase 2完了
│   ├── TimeSortMerger.hpp   ✅ Phase 3完了
│   └── CLIOperator.hpp      ✅ Phase 4完了
└── src/
    ├── DigitizerSource.cpp  ✅ Phase 2完了
    ├── FileWriter.cpp       ✅ Phase 2完了
    ├── TimeSortMerger.cpp   ✅ Phase 3完了
    └── CLIOperator.cpp      ✅ Phase 4完了

tests/
├── unit/
│   ├── core/
│   │   ├── test_idatacomponent.cpp    ✅ Phase 1完了
│   │   ├── test_ioperator.cpp         ✅ Phase 1完了
│   │   ├── test_component_config.cpp  ✅ Phase 1完了
│   │   └── test_operator_config.cpp   ✅ Phase 1完了
│   └── component/
│       ├── test_digitizer_source.cpp  ✅ Phase 2完了
│       ├── test_file_writer.cpp       ✅ Phase 2完了
│       ├── test_time_sort_merger.cpp  ✅ Phase 3完了
│       └── test_cli_operator.cpp      ✅ Phase 4完了
└── integration/
    ├── test_utils.hpp                        ✅ Phase 4完了（待機ユーティリティ）
    ├── test_source_writer_pipeline.cpp            ✅ Phase 2完了
    ├── test_multi_source_merger_pipeline.cpp      ✅ Phase 3完了
    └── test_operator_component_integration.cpp    ✅ Phase 5完了
```

---

## 6. 各フェーズの完了条件

### Phase 1 ✅ 完了
- [x] 全インターフェースのヘッダファイル作成
- [x] モッククラスでユニットテスト通過
- [x] IDataComponent: 入出力アドレス設定テスト通過
- [x] IOperator: Job管理テスト通過
- [x] ComponentConfig: 設定構造体テスト通過
- [x] OperatorConfig: 設定構造体テスト通過

### Phase 2 ✅ 完了
- [x] DigitizerSource: 状態遷移テスト通過（19テスト）
- [x] DigitizerSource: モックデータ生成テスト通過
- [x] DigitizerSource: ZMQ PUSH送信テスト通過
- [x] FileWriter: 状態遷移テスト通過（21テスト）
- [x] FileWriter: ZMQ PULL受信テスト通過
- [x] FileWriter: ファイル書き込みテスト通過
- [x] Source→Writer パイプラインテスト通過（15テスト）
- [x] エラー状態からの復帰テスト通過
- [x] 複数ラン連続実行テスト通過

### Phase 3 ✅ 完了
- [x] TimeSortMerger: 状態遷移テスト通過（23テスト）
- [x] TimeSortMerger: 複数入力受信テスト通過
- [x] 複数Source→Merger→Writer パイプラインテスト通過（14テスト）

### Phase 4 ✅ 完了
- [x] CLIOperator: IOperatorインターフェース実装
- [x] CLIOperator: 状態遷移テスト通過（36テスト）
- [x] CLIOperator: 非同期ジョブ実行テスト通過
- [x] CLIOperator: コンポーネント登録/解除テスト通過
- [x] test_utils.hpp: イベントベース待機ユーティリティ追加
- [x] 統合テスト: 実際の状態確認に移行（WaitForCondition使用）

### Phase 5 ✅ 完了
- [x] CLIOperator → DataComponents ZMQコマンド連携
- [x] 全コンポーネント統合テスト通過（7テスト）
- [x] GracefulStop動作確認
- [x] エラーリカバリ動作確認

---

## 7. 注意事項

### やること
- テストを先に書く（TDD）
- 最小限の実装から始める（KISS）
- 既存のlib/netコードを活用
- 各フェーズ完了後にレビュー

### やらないこと
- 「将来使うかも」の機能追加
- 過度な最適化
- テストなしのコード追加
- 複数フェーズの同時進行

---

## 8. 関連ドキュメント

- [00_OVERVIEW.md](00_OVERVIEW.md) - 全体概要
- [01_REQUIREMENTS.md](01_REQUIREMENTS.md) - 要求仕様
- [02_INTERFACE_DESIGN.md](02_INTERFACE_DESIGN.md) - インターフェース設計
- [04_TEST_STRATEGY.md](04_TEST_STRATEGY.md) - テスト戦略
