# Emulator コンポーネント設計

## 概要

Emulatorは、デジタイザハードウェアなしでシステムをテスト・デモするためのコンポーネントです。
実際のデジタイザと同じEventDataフォーマットでデータを生成します。

## 要件

### 機能要件
- EventDataと互換性のあるデータを乱数で生成
- 各Emulatorインスタンスは固定のModule番号を持つ
- 設定可能なイベント生成レート
- 設定可能なチャンネル数（0-63）
- タイムスタンプは単調増加

### 非機能要件
- DigitizerSourceと同じインターフェース（IDataComponent）
- 既存のパイプライン（SimpleMerger, FileWriter）と互換
- 低CPU負荷で高レート生成可能

## インターフェース

```cpp
// データモード
enum class EmulatorDataMode {
    Minimal,  // MinimalEventData (22 bytes) - 高速・軽量
    Full      // EventData (フル) - 波形データ含む
};

class Emulator : public IDataComponent {
public:
    // === データモード ===
    void SetDataMode(EmulatorDataMode mode);  // Minimal or Full（デフォルト: Minimal）

    // === 設定 ===
    void SetModuleNumber(uint8_t module);     // Module番号（必須）
    void SetNumChannels(uint8_t num);         // チャンネル数（デフォルト: 16）
    void SetEventRate(uint32_t rate);         // イベント/秒（デフォルト: 1000）
    void SetWaveformSize(size_t size);        // 波形サンプル数（Fullモード時、デフォルト: 0）

    // === 乱数パラメータ ===
    void SetEnergyRange(uint16_t min, uint16_t max);  // エネルギー範囲
    void SetSeed(uint64_t seed);              // 乱数シード（再現性のため）

    // IDataComponent interface
    // ... (Initialize, Run, Shutdown, etc.)
};
```

## 生成データ仕様

### データモード比較

| モード | データサイズ | 波形 | 用途 |
|--------|-------------|------|------|
| Minimal | 22 bytes | なし | 高速テスト、性能ベンチマーク |
| Full | 可変 | あり | 波形処理テスト、フル機能テスト |

### MinimalEventData フィールド（Minimalモード）

| フィールド | サイズ | 生成方法 |
|-----------|-------|---------|
| module | 1 byte | SetModuleNumber()で設定した固定値 |
| channel | 1 byte | 0 ~ (numChannels-1) の一様乱数 |
| energy | 2 bytes | SetEnergyRange()の範囲内の一様乱数 |
| energyShort | 2 bytes | energy * 0.8 程度 |
| timeStampNs | 8 bytes | 単調増加（レートから計算） |
| flags | 8 bytes | 0（通常は正常） |

### EventData フィールド（Fullモード）

| フィールド | 生成方法 |
|-----------|---------|
| timeStampNs | 単調増加（レートから計算） |
| module | SetModuleNumber()で設定した固定値 |
| channel | 0 ~ (numChannels-1) の一様乱数 |
| energy | SetEnergyRange()の範囲内の一様乱数 |
| energyShort | energy * 0.8 程度 |
| flags | 0（通常は正常） |
| waveformSize | 設定値（0でも可） |
| analogProbe1/2 | 波形有効時のみ乱数生成 |
| digitalProbe1-4 | 波形有効時のみ乱数生成 |
| その他 | デフォルト値 |

### タイムスタンプ生成

```cpp
// 基本レート: 1000 events/sec = 1ms間隔
// 実際のタイムスタンプ = 前のタイムスタンプ + (1e9 / rate) + jitter
// jitterは±10%程度のランダム変動
```

## 状態遷移

DigitizerSourceと同じ状態遷移:
```
Idle -> Configured -> Armed -> Running -> Configured
                                  |
                               Error -> Idle (Reset)
```

## 使用例

### Minimalモード（高速・軽量）

```cpp
auto emulator = std::make_unique<Emulator>();

// 設定
emulator->SetDataMode(EmulatorDataMode::Minimal);  // 22バイト/イベント
emulator->SetModuleNumber(0);        // Module 0
emulator->SetNumChannels(16);        // 16チャンネル
emulator->SetEventRate(100000);      // 100k events/sec（高速）
emulator->SetEnergyRange(0, 16383);  // 14bit ADC相当
emulator->SetOutputAddresses({"tcp://localhost:5555"});

// 初期化と実行
emulator->Initialize("");
emulator->Arm();
emulator->Start(1);  // Run 1

// ... データ生成中 ...

emulator->Stop(true);
```

### Fullモード（波形データ含む）

```cpp
auto emulator = std::make_unique<Emulator>();

// 設定
emulator->SetDataMode(EmulatorDataMode::Full);  // フルEventData
emulator->SetModuleNumber(0);
emulator->SetNumChannels(16);
emulator->SetEventRate(1000);        // 1k events/sec（波形生成は重い）
emulator->SetWaveformSize(1024);     // 1024サンプルの波形
emulator->SetEnergyRange(0, 16383);
emulator->SetOutputAddresses({"tcp://localhost:5555"});

emulator->Initialize("");
emulator->Arm();
emulator->Start(1);

// ... データ生成中 ...

emulator->Stop(true);
```

## 複数Emulatorの使用

```cpp
// 3モジュールのエミュレーション
std::vector<std::unique_ptr<Emulator>> emulators;
for (int i = 0; i < 3; ++i) {
    auto emu = std::make_unique<Emulator>();
    emu->SetModuleNumber(i);
    emu->SetOutputAddresses({"tcp://localhost:" + std::to_string(5555 + i)});
    emulators.push_back(std::move(emu));
}
```

## ファイル配置

```
lib/component/
├── include/
│   └── Emulator.hpp
└── src/
    └── Emulator.cpp

tests/unit/component/
└── test_emulator.cpp
```

## テスト項目

1. 初期状態テスト
2. 状態遷移テスト
3. Module番号固定テスト
4. チャンネル範囲テスト
5. イベントレートテスト
6. タイムスタンプ単調増加テスト
7. パイプライン統合テスト（Emulator -> SimpleMerger -> FileWriter）

## 実装順序

1. 基本クラス構造（IDataComponent継承）
2. 設定メソッド
3. イベント生成ロジック
4. ZMQ送信
5. ユニットテスト
6. 統合テスト
