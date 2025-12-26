# DELILA2 Clean Architecture リファクタリング計画

## 1. 現状の問題点

### 1.1 依存関係の問題
- `ZMQTransport`: nlohmann/json, zmq.hpp がヘッダーに露出
- `DataProcessor`: lz4 が直接依存、複数責任の混在
- インターフェース層の欠如でテスト・モック化が困難

### 1.2 単一責任原則 (SRP) 違反
- `DataProcessor`: シリアライズ + 圧縮 + チェックサム + シーケンス管理
- `ZMQTransport`: データ送受信 + ステータス送受信 + JSON シリアライズ

### 1.3 責任の混在
- `ComponentState`, `ComponentStatus` が `lib/net` に存在（DAQ全体の概念）
- ネットワーク層とドメイン層の分離が不十分

### 1.4 未実装機能
- Command通信 (REQ/REP)
- Metrics収集
- エラーハンドリング体系

---

## 2. 目標アーキテクチャ

```
┌─────────────────────────────────────────────────────────────────┐
│                         lib/core                                │
│  (ComponentState, ComponentStatus, Command, ErrorCode)          │
│  共通ドメインオブジェクト - 外部依存なし                          │
└─────────────────────────────────────────────────────────────────┘
                              ↑
                         依存
                              ↑
┌─────────────────────────────────────────────────────────────────┐
│                    lib/net Application Layer                    │
│  (CommandHandler, MetricsCollector, StateManager, DataPipeline) │
└─────────────────────────────────────────────────────────────────┘
                              ↑
                    依存関係逆転 (DIP)
                              ↑
┌─────────────────────────────────────────────────────────────────┐
│                   lib/net Interface Layer                       │
│  (IDataChannel, IStatusChannel, ICommandChannel)                │
│  (ISerializer, IChecksum)                                       │
└─────────────────────────────────────────────────────────────────┘
                              ↑
                         実装
                              ↑
┌─────────────────────────────────────────────────────────────────┐
│                  lib/net Infrastructure Layer                   │
│  (ZMQDataChannel, ZMQStatusChannel, ZMQCommandChannel)          │
│  (BinarySerializer, CRC32Checksum)                              │
└─────────────────────────────────────────────────────────────────┘
                              ↑
                         利用
                              ↑
┌─────────────────────────────────────────────────────────────────┐
│                      External Libraries                         │
│  (ZeroMQ, nlohmann/json)                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. 新しいディレクトリ構造

```
lib/
├── core/                       # 共通ドメイン（新規）
│   └── include/delila/core/
│       ├── ComponentState.hpp
│       ├── ComponentStatus.hpp
│       ├── Command.hpp
│       ├── CommandResponse.hpp
│       ├── ErrorCode.hpp
│       ├── EventData.hpp
│       └── MinimalEventData.hpp
│
├── digitizer/                  # 既存（変更なし）
│
└── net/
├── include/
│   ├── interfaces/              # インターフェース定義
│   │   ├── IDataChannel.hpp
│   │   ├── IStatusChannel.hpp
│   │   ├── ICommandChannel.hpp
│   │   ├── ISerializer.hpp
│   │   ├── ICompressor.hpp
│   │   └── IChecksum.hpp
│   │
│   ├── domain/                  # lib/net固有のドメイン
│   │   ├── DataHeader.hpp       # バイナリヘッダー定義
│   │   └── Metrics.hpp          # ネットワークメトリクス
│   │
│   ├── application/             # アプリケーションロジック
│   │   ├── StateManager.hpp     # 新規
│   │   ├── MetricsCollector.hpp # 新規
│   │   ├── CommandHandler.hpp   # 新規
│   │   └── DataPipeline.hpp     # DataProcessorを簡略化
│   │
│   ├── infrastructure/          # 外部ライブラリ実装
│   │   ├── zmq/
│   │   │   ├── ZMQContext.hpp
│   │   │   ├── ZMQDataChannel.hpp
│   │   │   ├── ZMQStatusChannel.hpp
│   │   │   └── ZMQCommandChannel.hpp
│   │   ├── serialization/
│   │   │   ├── BinarySerializer.hpp
│   │   │   └── JsonSerializer.hpp
│   │   └── checksum/
│   │       └── CRC32Checksum.hpp
│   │
│   ├── utils/                   # ユーティリティ（既存の良好なもの）
│   │   ├── SequenceGapDetector.hpp
│   │   ├── HeartbeatManager.hpp
│   │   ├── HeartbeatMonitor.hpp
│   │   ├── EOSTracker.hpp
│   │   └── TwoPhaseStartManager.hpp
│   │
│   └── facade/                  # 後方互換性のためのファサード
│       ├── ZMQTransport.hpp     # 既存APIを維持
│       └── DataProcessor.hpp    # 既存APIを維持
│
└── src/
    ├── domain/
    ├── application/
    ├── infrastructure/
    │   ├── zmq/
    │   ├── serialization/
    │   └── checksum/
    └── facade/
```

---

## 4. リファクタリングフェーズ

### Phase 0-Core: lib/core 共通ドメイン層 (約2時間)
**目的**: 共通ドメインオブジェクトを lib/core に分離

| ID | タスク | 見積もり |
|----|--------|---------|
| PC-1 | ディレクトリ構造作成 | 0.25h |
| PC-2 | ComponentState 移動・整理 | 0.25h |
| PC-3 | ComponentStatus 定義 | 0.25h |
| PC-4 | Command / CommandResponse 定義 | 0.5h |
| PC-5 | ErrorCode 定義 | 0.25h |
| PC-6 | CMakeLists.txt 作成 | 0.25h |
| PC-7 | 既存コードの更新 | 0.25h |

---

### Phase 0: lib/net 基盤整備 (約3.5時間)
**目的**: ディレクトリ構造とインターフェース定義

| ID | タスク | 見積もり |
|----|--------|---------|
| P0-1 | ディレクトリ構造作成 | 0.5h |
| P0-2 | IDataChannel インターフェース定義 | 0.5h |
| P0-3 | IStatusChannel インターフェース定義 | 0.5h |
| P0-4 | ICommandChannel インターフェース定義 | 0.5h |
| P0-5 | ISerializer インターフェース定義 | 0.5h |
| P0-6 | IChecksum インターフェース定義 | 0.5h |
| P0-7 | CMakeLists.txt 更新 | 0.5h |

---

### Phase 1: lib/net ドメイン層 (約1.5時間)
**目的**: lib/net固有のドメインオブジェクト

| ID | タスク | 見積もり |
|----|--------|---------|
| P1-1 | DataHeader 構造体定義 | 0.5h |
| P1-2 | Metrics 構造体定義 | 0.5h |
| P1-3 | ドメイン層ユニットテスト | 0.5h |

---

### Phase 2: Infrastructure層 - 分離 (約6時間)
**目的**: 既存コードを責任ごとに分離

| ID | タスク | 見積もり |
|----|--------|---------|
| P2-1 | CRC32Checksum クラス抽出 | 1h |
| P2-2 | BinarySerializer クラス抽出 | 2h |
| P2-3 | ZMQContext 共有クラス作成 | 0.5h |
| P2-4 | ZMQDataChannel クラス作成 | 1.5h |
| P2-5 | ZMQStatusChannel クラス作成 | 1h |
| P2-6 | Infrastructure層ユニットテスト（LZ4削除含む） | 1h |

---

### Phase 3: Infrastructure層 - Command通信 (約4時間)
**目的**: REQ/REP パターンの実装

| ID | タスク | 見積もり |
|----|--------|---------|
| P3-1 | ZMQCommandChannel クラス実装 | 2h |
| P3-2 | JsonSerializer (Command用) 実装 | 1h |
| P3-3 | Command通信ユニットテスト | 1h |

---

### Phase 4: Application層 (約6時間)
**目的**: ビジネスロジックの実装

| ID | タスク | 見積もり |
|----|--------|---------|
| P4-1 | StateManager 実装 | 1.5h |
| P4-2 | MetricsCollector 実装 | 1.5h |
| P4-3 | CommandHandler 実装 | 1.5h |
| P4-4 | DataPipeline (簡略化版) 実装 | 1h |
| P4-5 | Application層ユニットテスト | 0.5h |

---

### Phase 5: Facade層 - 後方互換性 (約3時間)
**目的**: 既存APIの維持

| ID | タスク | 見積もり |
|----|--------|---------|
| P5-1 | ZMQTransport ファサード作成 | 1.5h |
| P5-2 | DataProcessor ファサード作成 | 1h |
| P5-3 | 既存テストの動作確認 | 0.5h |

---

### Phase 6: 統合とテスト (約4時間)
**目的**: 全体の統合テスト

| ID | タスク | 見積もり |
|----|--------|---------|
| P6-1 | 統合テスト作成 | 2h |
| P6-2 | サンプルコード更新 | 1h |
| P6-3 | ドキュメント更新 | 1h |

---

## 5. 見積もりサマリー

| Phase | 内容 | 見積もり |
|-------|------|---------|
| Phase 0-Core | lib/core 共通ドメイン | 2時間 |
| Phase 0 | lib/net 基盤整備 | 3.5時間 |
| Phase 1 | lib/net ドメイン層 | 1.5時間 |
| Phase 2 | Infrastructure層 - 分離 | 6時間 |
| Phase 3 | Infrastructure層 - Command | 4時間 |
| Phase 4 | Application層 | 6時間 |
| Phase 5 | Facade層 | 3時間 |
| Phase 6 | 統合とテスト | 4時間 |
| **合計** | | **30時間** |

**注意**:
- LZ4圧縮は削除対象。既存のDataProcessorから圧縮機能を除去。
- `ComponentState`, `ComponentStatus`, `Command` 等は `lib/core` に移動。

---

## 6. テスト戦略

### 方針: 新規作成で置き換え

既存テストは新しいアーキテクチャと互換性がないため、**新規テストを作成し、既存テストを削除**する。

### 削除対象の既存テスト

| 削除対象 | Phase | 理由 |
|---------|-------|------|
| `tests/unit/net/test_component_state.cpp` | 0-Core | lib/core に移動、namespace変更 |
| `tests/unit/net/test_message_type.cpp` | 1 | 新アーキテクチャで不要 |
| `tests/unit/net/test_serializer_format_versions.cpp` | 2 | 新 BinarySerializer テストで置き換え |
| `tests/unit/net/test_data_processor.cpp` | 2/5 | Facade テストで置き換え |
| `tests/unit/net/test_data_processor_sequence.cpp` | 2/5 | Facade テストで置き換え |
| `tests/unit/net/test_zmq_transport_bytes.cpp` | 2 | 新 ZMQDataChannel テストで置き換え |
| `tests/unit/net/test_zmq_transport_minimal.cpp` | 2 | 新チャネルテストで置き換え |
| `tests/unit/net/test_two_phase_start.cpp` | 4 | StateManager テストで置き換え |

### 維持する既存テスト

以下のユーティリティテストは新アーキテクチャでも使用するため**維持**:

- `tests/unit/net/test_sequence_gap_detector.cpp`
- `tests/unit/net/test_heartbeat_manager.cpp`
- `tests/unit/net/test_heartbeat_monitor.cpp`
- `tests/unit/net/test_eos_tracker.cpp`
- `tests/unit/core/test_minimal_event_data.cpp`

### 新規テスト構造

```
tests/
├── unit/
│   ├── core/                           # lib/core テスト
│   │   ├── test_component_state.cpp
│   │   ├── test_command.cpp
│   │   ├── test_command_response.cpp
│   │   ├── test_error_code.cpp
│   │   └── test_minimal_event_data.cpp
│   │
│   └── net/
│       ├── interfaces/                 # インターフェースコンパイルテスト
│       │   └── test_interface_compilation.cpp
│       │
│       ├── domain/                     # ドメイン層テスト
│       │   ├── test_component_status.cpp
│       │   ├── test_metrics.cpp
│       │   └── test_data_header.cpp
│       │
│       ├── infrastructure/             # Infrastructure層テスト
│       │   ├── test_crc32_checksum.cpp
│       │   ├── test_binary_serializer.cpp
│       │   ├── test_zmq_data_channel.cpp
│       │   ├── test_zmq_status_channel.cpp
│       │   └── test_zmq_command_channel.cpp
│       │
│       ├── application/                # Application層テスト
│       │   ├── test_state_manager.cpp
│       │   ├── test_metrics_collector.cpp
│       │   └── test_data_pipeline.cpp
│       │
│       ├── facade/                     # Facade層テスト
│       │   ├── test_zmq_transport_facade.cpp
│       │   └── test_data_processor_facade.cpp
│       │
│       └── utils/                      # ユーティリティテスト（既存維持）
│           ├── test_sequence_gap_detector.cpp
│           ├── test_heartbeat_manager.cpp
│           ├── test_heartbeat_monitor.cpp
│           └── test_eos_tracker.cpp
│
├── integration/                        # 統合テスト
│   ├── test_command_integration.cpp
│   ├── test_data_pipeline_integration.cpp
│   └── test_full_acquisition_cycle.cpp
│
└── benchmarks/                         # パフォーマンステスト
    └── bench_data_pipeline.cpp
```

### Phase別テスト実装

| Phase | 新規作成 | 削除 |
|-------|---------|------|
| 0-Core | core/ 下 4ファイル | net/test_component_state.cpp |
| 0 | interfaces/ 下 1ファイル | なし |
| 1 | domain/ 下 3ファイル | test_message_type.cpp |
| 2 | infrastructure/ 下 4ファイル | 5ファイル |
| 3 | infrastructure/ 下 1ファイル | なし |
| 4 | application/ 下 3ファイル | test_two_phase_start.cpp |
| 5 | facade/ 下 2ファイル | なし |
| 6 | integration/ 下 3ファイル | なし |

---

## 7. リスクと対策

### リスク1: 既存テストの破壊
**対策**: 新しいアーキテクチャ用のテストを各Phaseで作成。Facade層テストで後方互換性を確認。

### リスク2: 作業量の増大
**対策**: 各Phaseを独立させ、途中でも動作する状態を維持

### リスク3: 複雑化
**対策**: KISS原則を維持、過度な抽象化を避ける

---

## 8. 成功基準

1. **全既存テストがパス**
2. **新規機能（Command通信）が動作**
3. **各クラスが単一責任**
4. **外部ライブラリ依存がInfrastructure層に限定**
5. **モックを使ったテストが可能**

---

## 8. 実装順序の理由

```
Phase 0-Core (共通) → Phase 0 (基盤) → Phase 1 (ドメイン) → Phase 2 (分離)
                                                                  ↓
           Phase 5 (互換性) ← Phase 4 (アプリ) ← Phase 3 (Command)
                                                                  ↓
                                                           Phase 6 (統合)
```

- **Phase 0-Core**: 共通ドメインを最初に分離（他のすべてが依存）
- **Phase 0-1**: lib/net のインターフェースとドメイン
- **Phase 2**: 既存コードの責任分離（最も工数が大きい）
- **Phase 3**: 新機能追加（Command通信）
- **Phase 4**: ビジネスロジック
- **Phase 5**: 後方互換性の確保
- **Phase 6**: 全体統合

---

## 9. 次のステップ

1. この計画の承認
2. Phase 0 から開始
3. 各Phase完了時にレビュー

承認いただければ、Phase 0 の詳細タスクリストを作成します。
