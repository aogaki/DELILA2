# MonitorROOT コンポーネント設計

## 概要

MonitorROOTは、データストリームを受信してROOTヒストグラムを生成し、
THttpServerを通じてWebブラウザでリアルタイム表示するコンポーネントです。

## 要件

### 機能要件
- ZMQ PULLでデータ受信（SimpleMergerの出力をサブスクライブ）
- EventDataからヒストグラムを生成
- THttpServerでWebブラウザからアクセス可能
- ラン開始時にヒストグラムをリセット

### 非機能要件
- IDataComponent準拠
- ROOTに依存（CMakeでオプショナル）
- 高レートでもドロップせずに処理

## 依存関係

- ROOT 6.x（TH1, TH2, TGraph, THttpServer）
- 既存: ZMQTransport, DataProcessor

## インターフェース

```cpp
class MonitorROOT : public IDataComponent {
public:
    // === 設定 ===
    void SetHttpPort(int port);              // HTTPサーバーポート（デフォルト: 8080）
    void SetUpdateInterval(uint32_t ms);     // ヒストグラム更新間隔（デフォルト: 1000）

    // === ヒストグラム設定 ===
    void EnableEnergyHistogram(bool enable);
    void EnableChannelHistogram(bool enable);
    void EnableTimingHistogram(bool enable);
    void Enable2DHistogram(bool enable);     // Energy vs Channel

    // === 波形表示設定 ===
    void EnableWaveformDisplay(bool enable); // TGraphで波形表示
    void SetWaveformChannel(uint8_t module, uint8_t channel);  // 表示する波形のチャンネル

    // IDataComponent interface
    // ... (Initialize, Run, Shutdown, etc.)
};
```

## ヒストグラム仕様

### 基本ヒストグラム

| 名前 | タイプ | X軸 | Y軸 | 説明 |
|------|--------|-----|-----|------|
| hEnergy | TH1F | 0-16384 | Counts | 全チャンネルのエネルギー分布 |
| hChannel | TH1I | 0-64 | Counts | チャンネル別イベント数 |
| hModule | TH1I | 0-16 | Counts | モジュール別イベント数 |
| hTimeDiff | TH1F | 0-1000 us | Counts | イベント間隔分布 |

### 2Dヒストグラム

| 名前 | タイプ | X軸 | Y軸 | 説明 |
|------|--------|-----|-----|------|
| hEnergyVsChannel | TH2F | Channel | Energy | チャンネル別エネルギー |
| hEnergyVsModule | TH2F | Module | Energy | モジュール別エネルギー |

### チャンネル別ヒストグラム（オプション）

```cpp
// hEnergy_mod0_ch0, hEnergy_mod0_ch1, ... のような
// 個別チャンネルヒストグラムはオプション
void EnablePerChannelHistograms(bool enable);
```

### 波形グラフ（TGraph）

| 名前 | タイプ | X軸 | Y軸 | 説明 |
|------|--------|-----|-----|------|
| gWaveform | TGraph | Sample | ADC値 | 選択チャンネルの波形表示 |

**TCanvasは不要**: THttpServerはTGraphを直接登録でき、JSROOTがブラウザ側で
自動的に描画を処理する。TH1/TH2と同様に、オブジェクトを登録するだけでよい。

## THttpServer設定

```cpp
// サーバー起動
fServer = std::make_unique<THttpServer>("http:8080");

// ヒストグラム登録
fServer->Register("/Histograms/Energy", hEnergy);
fServer->Register("/Histograms/Channel", hChannel);
fServer->Register("/Histograms/2D/EnergyVsChannel", hEnergyVsChannel);

// 波形グラフ登録（TCanvas不要、直接登録可能）
fServer->Register("/Waveform", gWaveform);
```

### 表示更新について

THttpServerはJSROOTを使用してブラウザ側で描画を処理する。
サーバー側でオブジェクトを更新すれば、ブラウザがリクエスト時に最新データを取得する。

```cpp
// 波形更新
void UpdateWaveform(const EventData& event) {
    gWaveform->Set(event.waveformSize);
    for (size_t i = 0; i < event.waveformSize; ++i) {
        gWaveform->SetPoint(i, i, event.analogProbe1[i]);
    }
    // TCanvas::Update()は不要 - JSROOTが処理
}
```

**参考**: [ROOT THttpServer Documentation](https://root.cern/doc/master/classTHttpServer.html)

## 状態遷移

```
Idle -> Configured -> Armed -> Running -> Configured
                                  |
                               Error -> Idle (Reset)
```

### 状態別動作

| 状態 | THttpServer | データ受信 | ヒストグラム更新 |
|------|-------------|-----------|-----------------|
| Idle | 停止 | × | × |
| Configured | 起動 | × | 表示のみ |
| Armed | 起動 | × | 表示のみ |
| Running | 起動 | ○ | ○ |

## アーキテクチャ

```
┌────────────────────────────────────────────┐
│ MonitorROOT                                │
│                                            │
│  ┌──────────┐   ┌──────────┐   ┌────────┐ │
│  │ReceiveThread│→│ProcessThread│→│Histograms│ │
│  │(ZMQ PULL) │   │(Fill)    │   │        │ │
│  └──────────┘   └──────────┘   └────┬───┘ │
│                                     │      │
│                              ┌──────▼────┐ │
│                              │THttpServer│ │
│                              │ :8080     │ │
│                              └───────────┘ │
└────────────────────────────────────────────┘
           ▲
           │ ZMQ PULL
           │
    ┌──────┴──────┐
    │SimpleMerger │
    │(or Emulator)│
    └─────────────┘
```

## 使用例

```cpp
auto monitor = std::make_unique<MonitorROOT>();

// 設定
monitor->SetHttpPort(8080);
monitor->SetUpdateInterval(500);  // 500ms更新
monitor->SetInputAddresses({"tcp://localhost:5560"});

// ヒストグラム有効化
monitor->EnableEnergyHistogram(true);
monitor->EnableChannelHistogram(true);
monitor->Enable2DHistogram(true);

// 初期化と実行
monitor->Initialize("");
monitor->Arm();
monitor->Start(1);

// ブラウザで http://localhost:8080 にアクセス

monitor->Stop(true);
```

## ファイル配置

```
lib/component/
├── include/
│   └── MonitorROOT.hpp
└── src/
    └── MonitorROOT.cpp

tests/unit/component/
└── test_monitor_root.cpp (ROOTがある場合のみ)
```

## CMake設定

```cmake
# ROOTを探す
find_package(ROOT QUIET COMPONENTS Core Hist Graf HttpServer)

if(ROOT_FOUND)
    message(STATUS "ROOT found - building MonitorROOT")
    list(APPEND COMPONENT_SOURCES src/MonitorROOT.cpp)
    list(APPEND COMPONENT_HEADERS include/MonitorROOT.hpp)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ROOT::Core ROOT::Hist ROOT::Graf ROOT::HttpServer)
else()
    message(STATUS "ROOT not found - MonitorROOT will not be built")
endif()
```

**必要なROOTコンポーネント:**
- `Core`: 基本機能
- `Hist`: TH1, TH2
- `Graf`: TGraph（TCanvasは不要だがTGraphはGrafに含まれる）
- `HttpServer`: THttpServer

## テスト項目

1. 初期状態テスト
2. 状態遷移テスト
3. THttpServerポート設定テスト
4. ヒストグラム生成テスト（TH1/TH2）
5. 波形グラフテスト（TGraph）
6. データ受信・Fill テスト
7. ラン切り替え時リセットテスト
8. 統合テスト（Emulator -> MonitorROOT）

## 実装順序

1. 基本クラス構造（IDataComponent継承）
2. THttpServer起動/停止
3. 基本ヒストグラム（Energy, Channel）
4. データ受信・Fill
5. 2Dヒストグラム
6. 波形グラフ（TGraph）
7. ユニットテスト
8. 統合テスト

## 注意事項

- ROOT のスレッド安全性: `ROOT::EnableThreadSafety()` を呼ぶ（ROOT 6.x）
- TH1/TH2/TGraph: THttpServer + JSROOTが自動的に描画を処理
- TCanvasは不要: オブジェクトを直接Register()するだけでよい
- 高レート時: バッチ処理でFillを効率化
