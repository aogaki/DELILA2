# DELILA2 コンポーネントフレームワーク設計

## 実装状況

| フェーズ | 状態 | 内容 |
|----------|------|------|
| Phase 1 | ✅ 完了 | インターフェース定義 |
| Phase 2 | ✅ 完了 | DigitizerSource → FileWriter パイプライン |
| Phase 3 | ✅ 完了 | TimeSortMerger |
| Phase 4 | ✅ 完了 | CLIOperator |
| Phase 5 | ✅ 完了 | 統合テスト（Operator + DataComponents連携） |

**テスト結果**: 全テスト通過（ユニットテスト329 + 統合テスト57）

## 概要

既存のライブラリ（Digitizer, Network）を活用し、再利用可能なDAQコンポーネントフレームワークを構築する。

## 設計思想

### 階層構造

```
IComponent (状態管理、コマンド、ステータス)
    │
    ├── IDataComponent (データ入出力)
    │       │
    │       ├── DigitizerSource (in:0, out:1)
    │       ├── FileWriter (in:1, out:0)
    │       ├── OnlineAnalyzer (in:1, out:0)
    │       └── TimeSortMerger (in:N, out:1)
    │
    └── IOperator (システム制御)
            │
            └── CLIOperator
```

### 原則

1. **KISS**: 必要最小限の機能から開始
2. **Composition**: 継承より構成（has-a）
3. **インターフェース分離**: 各コンポーネントは明確な責務を持つ
4. **テスト駆動開発**: インターフェースごとにテストを先に作成

## コンポーネント構成

IDataComponentは入出力アドレスの数で役割が決まる：

| コンポーネント | 入力数 | 出力数 | 用途 | 状態 |
|----------------|--------|--------|------|------|
| DigitizerSource | 0 | 1 | ハードウェアからデータ取得 | ✅ 実装完了 |
| FileWriter | 1 | 0 | ファイルへデータ書き込み | ✅ 実装完了 |
| OnlineAnalyzer | 1 | 0 | リアルタイム解析 | 未実装 |
| TimeSortMerger | N | 1 | 複数ソースを時系列ソート | ✅ 実装完了 |

## 内部構成（Composition）

```cpp
class DigitizerSource : public IDataComponent {
private:
    std::unique_ptr<IDigitizer> fDigitizer;      // データ取得
    std::unique_ptr<ZMQTransport> fTransport;    // ネットワーク通信
    std::unique_ptr<DataProcessor> fProcessor;   // シリアライズ
};

class FileWriter : public IDataComponent {
private:
    std::unique_ptr<ZMQTransport> fTransport;    // ネットワーク通信
    std::unique_ptr<DataProcessor> fProcessor;   // デシリアライズ
    std::unique_ptr<FileHandle> fFile;           // ファイル出力
};
```

## 状態遷移

```
Idle → Configuring → Configured → Arming → Armed → Starting → Running → Stopping → Configured
                                                                              ↓
                                                          Any state → Error → Idle
```

## データフロー

```
[DigitizerSource] ──┐
                    ├──▶ [TimeSortMerger] ──▶ [FileWriter]
[DigitizerSource] ──┘
                              ▲
                              │ Command/Status
                        [Operator]
```

## 通信

| チャネル | パターン | 用途 |
|----------|----------|------|
| コマンド | REQ/REP | Operator → Component 制御 |
| データ | PUSH/PULL | Component間データ転送 |
| ステータス | PUB/SUB | Component → Operator 状態報告 |

## ドキュメント構成

| ファイル | 内容 | 状態 |
|----------|------|------|
| [00_OVERVIEW.md](00_OVERVIEW.md) | 全体概要（本ファイル） | 更新済 |
| [01_REQUIREMENTS.md](01_REQUIREMENTS.md) | 要求仕様 | 完了 |
| [02_INTERFACE_DESIGN.md](02_INTERFACE_DESIGN.md) | インターフェース設計 | 完了 |
| [03_IMPLEMENTATION_PLAN.md](03_IMPLEMENTATION_PLAN.md) | 実装計画 | Phase 5完了時点で更新 |
| [04_TEST_STRATEGY.md](04_TEST_STRATEGY.md) | テスト戦略 | 完了 |
| [05_REMAINING_TASKS.md](05_REMAINING_TASKS.md) | 残タスク一覧 | 新規追加 |

## 関連ドキュメント

- [CLAUDE.md](../CLAUDE.md) - 開発ガイドライン
- [DESIGN.md](../DESIGN.md) - 全体設計
- [lib/net/README.md](../lib/net/README.md) - ネットワークライブラリ
