# DELILA2 Clean Architecture リファクタリング

## 概要

本プロジェクトは、DELILA2 を Clean Architecture に基づいてリファクタリングします：
- **lib/core**: 共通ドメインオブジェクト（ComponentState, Command等）を新設
- **lib/net**: ネットワーク層をClean Architectureに再構成
- **LZ4圧縮削除**: 不要な圧縮機能を完全に除去

---

## ドキュメント一覧

| ファイル | 内容 | 見積もり |
|---------|------|---------|
| [Architecture_Refactoring_Plan.md](Architecture_Refactoring_Plan.md) | 全体計画 | - |
| [Phase0_Core.md](Phase0_Core.md) | lib/core 共通ドメイン層 | 2時間 |
| [Phase0_Foundation.md](Phase0_Foundation.md) | lib/net 基盤整備 | 3.5時間 |
| [Phase1_Domain.md](Phase1_Domain.md) | lib/net ドメイン層 | 1.5時間 |
| [Phase2_Infrastructure_Separation.md](Phase2_Infrastructure_Separation.md) | Infrastructure層（既存分離） | 6時間 |
| [Phase3_Command_Channel.md](Phase3_Command_Channel.md) | Infrastructure層（Command通信） | 4時間 |
| [Phase4_Application.md](Phase4_Application.md) | Application層 | 6時間 |
| [Phase5_Facade.md](Phase5_Facade.md) | Facade層（後方互換性） | 3時間 |
| [Phase6_Integration.md](Phase6_Integration.md) | 統合テスト | 4時間 |

**総見積もり時間**: 30時間

---

## アーキテクチャ

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
```

---

## 実装順序

```
Phase 0-Core: lib/core 共通ドメイン
    ├── ComponentState, ComponentStatus
    ├── Command, CommandResponse
    └── ErrorCode
         │
         ↓
Phase 0: lib/net 基盤整備
    ├── ディレクトリ構造
    └── インターフェース定義
         │
         ↓
Phase 1: lib/net ドメイン層
    ├── DataHeader
    └── Metrics
         │
         ↓
Phase 2: Infrastructure層（分離）
    ├── CRC32Checksum
    ├── BinarySerializer
    ├── ZMQDataChannel
    └── ZMQStatusChannel
         │
         ↓
Phase 3: Infrastructure層（Command）
    └── ZMQCommandChannel
         │
         ↓
Phase 4: Application層
    ├── StateManager
    ├── MetricsCollector
    ├── CommandHandler
    └── DataPipeline
         │
         ↓
Phase 5: Facade層
    ├── ZMQTransport（後方互換）
    └── DataProcessor（後方互換）
         │
         ↓
Phase 6: 統合テスト
    ├── 統合テスト
    ├── サンプル更新
    └── ドキュメント
```

---

## 新しいディレクトリ構造

```
lib/
├── core/                       # 共通ドメイン（新規）
│   └── include/delila/core/
│       ├── ComponentState.hpp
│       ├── ComponentStatus.hpp
│       ├── Command.hpp
│       ├── CommandResponse.hpp
│       └── ErrorCode.hpp
│
└── net/
    ├── include/
    │   ├── interfaces/         # インターフェース定義
    │   │   ├── IDataChannel.hpp
    │   │   ├── IStatusChannel.hpp
    │   │   ├── ICommandChannel.hpp
    │   │   ├── ISerializer.hpp
    │   │   └── IChecksum.hpp
    │   │
    │   ├── domain/             # lib/net固有のドメイン
    │   │   ├── DataHeader.hpp
    │   │   └── Metrics.hpp
    │   │
    │   ├── application/        # アプリケーションロジック
    │   │   ├── StateManager.hpp
    │   │   ├── MetricsCollector.hpp
    │   │   ├── CommandHandler.hpp
    │   │   └── DataPipeline.hpp
    │   │
    │   ├── infrastructure/     # 外部ライブラリ実装
    │   │   ├── zmq/
    │   │   ├── serialization/
    │   │   └── checksum/
    │   │
    │   ├── utils/              # ユーティリティ（既存）
    │   │
    │   └── facade/             # 後方互換性
    │       ├── ZMQTransport.hpp
    │       └── DataProcessor.hpp
    │
    └── src/
        └── (同様の構造)
```

---

## 成功基準

1. **既存テスト全パス** - 後方互換性の維持
2. **新機能の動作** - Command通信、Metrics収集
3. **単一責任** - 各クラスが1つの責任のみ
4. **依存関係逆転** - Infrastructure → Interface ← Application
5. **テスト容易性** - モックを使ったテストが可能

---

## 旧計画

以前の計画（リファクタリング前）は [Next/](Next/) に保存されています。

---

## 進捗管理

各Phaseの完了時にチェック:

- [ ] Phase 0-Core: lib/core 共通ドメイン
- [ ] Phase 0: lib/net 基盤整備
- [ ] Phase 1: lib/net ドメイン層
- [ ] Phase 2: Infrastructure層（分離）
- [ ] Phase 3: Infrastructure層（Command）
- [ ] Phase 4: Application層
- [ ] Phase 5: Facade層
- [ ] Phase 6: 統合テスト
