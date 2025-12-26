# DELILA2 コンポーネント実装タスクリスト

本ディレクトリには、DELILA2 ネットワークコンポーネントの実装タスクリストが含まれています。

---

## ドキュメント一覧

| ファイル | 内容 | 優先度 | 見積もり時間 |
|---------|------|--------|-------------|
| [Phase1_Command_Communication.md](Phase1_Command_Communication.md) | Command通信 (REQ/REP) | 最高 | 約14時間 |
| [Phase2_Metrics_Collection.md](Phase2_Metrics_Collection.md) | メトリクス収集 | 高 | 約10.5時間 |
| [Phase3_Error_Handling.md](Phase3_Error_Handling.md) | エラーハンドリング | 高 | 約6.5時間 |
| [Phase4_IComponent_Interface.md](Phase4_IComponent_Interface.md) | IComponentインターフェース | 中 | 約5時間 |

**総見積もり時間**: 約36時間

---

## 実装順序

```
Phase 1: Command通信基盤
    │
    ├── C-1: Command構造体定義
    ├── C-2: CommandResponse構造体定義
    ├── C-3: ErrorCode enum定義 ──────────────────────┐
    │                                                 │
    ├── C-4: Command JSONシリアライズ/デシリアライズ   │
    ├── C-5: ZMQTransportにCommandSocket追加         │
    ├── C-6: SendCommand/ReceiveCommand実装          │
    ├── C-7: タイムアウト処理実装                     │
    ├── C-8: Command通信ユニットテスト               │
    └── C-9: Command通信統合テスト                   │
                                                     │
Phase 2: Metrics収集                                 │
    │                                                 │
    ├── M-1: Metrics構造体定義                        │
    ├── M-2: MetricsCollectorクラス実装              │
    ├── M-3: ComponentStatusにMetrics統合            │
    ├── M-4: 定期送信機構実装                        │
    ├── M-5: Metricsユニットテスト                   │
    └── M-6: Metrics統合テスト                       │
                                                     │
Phase 3: Error Handling ◀────────────────────────────┘
    │         (C-3と統合)
    ├── E-1: ErrorCategory/ErrorCode追加機能
    ├── E-2: ComponentStateにERROR追加
    ├── E-3: 状態遷移ロジック更新
    ├── E-4: CONFIGUREでのERRORリカバリ
    └── E-5: エラーハンドリングユニットテスト

Phase 4: IComponent インターフェース
    │
    ├── I-1: IComponentインターフェース定義
    ├── I-2: コマンドハンドラ基本実装
    └── I-3: IComponentテスト（モック使用）

    ↓

コンポーネント実装（Phase 5以降）
    ├── SourceComponent
    └── SinkComponent
```

---

## クイックリファレンス

### 新規作成ファイル一覧

#### Phase 1
- `lib/net/include/Command.hpp`
- `lib/net/src/Command.cpp`
- `lib/net/include/ErrorCode.hpp`
- `lib/net/src/ErrorCode.cpp`
- `tests/unit/net/test_command.cpp`
- `tests/unit/net/test_command_serialization.cpp`
- `tests/unit/net/test_error_code.cpp`
- `tests/integration/test_command_integration.cpp`

#### Phase 2
- `lib/net/include/Metrics.hpp`
- `lib/net/src/Metrics.cpp`
- `lib/net/include/MetricsCollector.hpp`
- `lib/net/src/MetricsCollector.cpp`
- `lib/net/include/MetricsReporter.hpp`
- `lib/net/src/MetricsReporter.cpp`
- `tests/unit/net/test_metrics.cpp`
- `tests/unit/net/test_metrics_collector.cpp`
- `tests/unit/net/test_metrics_reporter.cpp`
- `tests/integration/test_metrics_integration.cpp`

#### Phase 3
- `lib/net/include/StateManager.hpp`
- `lib/net/src/StateManager.cpp`
- `tests/unit/net/test_state_manager.cpp`
- `tests/unit/net/test_error_handling.cpp`

#### Phase 4
- `lib/net/include/IComponent.hpp`
- `lib/net/include/ComponentCommandHandler.hpp`
- `lib/net/src/ComponentCommandHandler.cpp`
- `tests/unit/net/test_icomponent.cpp`

### 更新ファイル一覧

- `lib/net/include/ZMQTransport.hpp` - CommandSocket追加、ComponentStatus更新
- `lib/net/src/ZMQTransport.cpp` - コマンド送受信メソッド追加
- `lib/net/include/ComponentState.hpp` - Error状態追加、遷移関数追加
- `tests/unit/net/test_component_state.cpp` - Error状態テスト追加
- `tests/CMakeLists.txt` - 新規テストファイル追加

---

## 開発フロー

各タスクは以下のTDDフローで実装：

1. **テストを書く** (RED)
   - 期待する動作をテストとして記述
   - テストが失敗することを確認

2. **最小限の実装** (GREEN)
   - テストが通る最小限のコードを書く
   - 機能が動作することを確認

3. **リファクタリング** (REFACTOR)
   - コードを整理・改善
   - テストが引き続き通ることを確認

4. **レビュー・コミット**
   - コード品質確認
   - ドキュメント更新
   - コミット

---

## 完了チェックリスト

### Phase 1 完了条件
- [ ] 全ユニットテスト合格
- [ ] 統合テスト合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

### Phase 2 完了条件
- [ ] 全ユニットテスト合格
- [ ] 統合テスト合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

### Phase 3 完了条件
- [ ] 全ユニットテスト合格
- [ ] 既存テスト引き続き合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

### Phase 4 完了条件
- [ ] 全ユニットテスト合格
- [ ] モックテスト合格
- [ ] コンパイル警告なし
- [ ] ドキュメント更新

---

## 関連ドキュメント

- [要求仕様書](../docs/requirements/component_requirements.md)
- [CLAUDE.md](../CLAUDE.md) - TDDガイドライン
- [PreSearch/net/DESIGN.md](../PreSearch/net/DESIGN.md) - ネットワーク設計

---

## 備考

- 各Phaseは依存関係があるため、順序通りに実装する
- Phase 1のC-3とPhase 3のE-1は統合して実装可能
- テストファイル名は`test_*.cpp`の形式で統一
- 見積もり時間は目安であり、実際の時間は異なる場合がある
