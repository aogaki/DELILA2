# 開発原則

このファイルはアーカイブ対象外です。全ての実装作業において遵守すべき原則を定義します。

---

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

## ドキュメント管理

### 実装中のルール
1. 各フェーズ・タスクごとにTODOディレクトリのドキュメントを更新する
2. 進捗状況、完了項目、変更点を記録する
3. 設計変更があれば設計ドキュメントを更新する

### 実装完了時のルール
1. 全ての実装が完了したら、このファイル（PRINCIPLES.md）を除く全てのドキュメントをアーカイブする
2. `archive/` ディレクトリ配下に新しいディレクトリを作成する
3. ディレクトリ名はTODOの内容から生成された適切な名前を使用する
   - 例: `component_framework_phase1-5`, `emulator_monitor_implementation`

### ディレクトリ構造
```
TODO/
├── PRINCIPLES.md          # この原則ファイル（アーカイブ対象外）
├── 00_OVERVIEW.md         # 現在の作業概要
├── 01_xxx.md              # 設計・計画ドキュメント
├── 02_xxx.md
└── archive/               # 過去のTODOリスト
    ├── component_framework_phase1-5/
    │   ├── 00_OVERVIEW.md
    │   ├── 01_REQUIREMENTS.md
    │   └── ...
    └── [次のアーカイブ]/
```

---

## コーディング規約

### C++ スタイル
- C++17準拠
- 変数名: camelCase（メンバ変数は`f`プレフィックス）
- クラス名: PascalCase
- 定数: kPascalCase または SCREAMING_SNAKE_CASE

### テスト命名
- テストファイル: `test_<component_name>.cpp`
- テストケース: `<ClassName>Test`
- テスト名: `<ActionOrCondition>_<ExpectedResult>` または説明的な名前

---

## 品質基準

### コミット前チェック
- [ ] 全てのユニットテストがパス
- [ ] 全ての統合テストがパス
- [ ] 新規コードにはテストが存在
- [ ] ドキュメントが更新されている

### レビュー基準
- KISS原則に従っているか
- テストが十分か
- エラーハンドリングが適切か
