# Emulator & MonitorROOT 実装

> **注意**: 全ての実装は [PRINCIPLES.md](PRINCIPLES.md) に従って行う（KISS原則、TDD）

## 概要

新しいコンポーネントの追加:
1. **Emulator** - デジタイザのエミュレーター（テスト・デモ用）
2. **MonitorROOT** - ROOT/THttpServerを使用したオンラインモニター

## 現在のコンポーネント構成

```
lib/component/
├── DigitizerSource.hpp/cpp  - 実ハードウェアからのデータ取得
├── SimpleMerger.hpp/cpp     - 複数入力のマージ（ソートなし）
├── FileWriter.hpp/cpp       - ファイル書き込み
└── CLIOperator.hpp/cpp      - CLI制御オペレーター
```

## 追加予定のコンポーネント

### Emulator
- デジタイザと同じデータフォーマットを乱数で生成
- 各Emulatorインスタンスは固定のModule番号を持つ
- テスト・デモ・開発時のハードウェア代替

### MonitorROOT
- データを受信してROOTヒストグラムを生成
- THttpServerでWebブラウザからヒストグラムを閲覧可能
- リアルタイムモニタリング用

## 想定パイプライン

```
Emulator(mod=0) ─┐
Emulator(mod=1) ─┼─→ SimpleMerger ─→ FileWriter
Emulator(mod=2) ─┘         │
                           └─→ MonitorROOT (THttpServer)
```

## 関連ドキュメント

- [PRINCIPLES.md](PRINCIPLES.md) - 開発原則（KISS、TDD、ドキュメント管理）
- [01_EMULATOR_DESIGN.md](01_EMULATOR_DESIGN.md) - Emulator設計
- [02_MONITOR_ROOT_DESIGN.md](02_MONITOR_ROOT_DESIGN.md) - MonitorROOT設計

## アーカイブ

過去のTODOは以下に保存:
- [archive/component_framework_phase1-5/](archive/component_framework_phase1-5/) - コンポーネントフレームワーク Phase 1-5
