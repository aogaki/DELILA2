# DELILA2 詳細日本語マニュアル

## 目次
1. [プロジェクト概要](#1-プロジェクト概要)
2. [環境構築](#2-環境構築)
3. [ビルド手順](#3-ビルド手順)
4. [基本的な使い方](#4-基本的な使い方)
5. [実践的なサンプルプログラム](#5-実践的なサンプルプログラム)
6. [DAQテストシステム](#6-daqテストシステム)
7. [ネットワークライブラリのサンプル](#7-ネットワークライブラリのサンプル)
8. [トラブルシューティング](#8-トラブルシューティング)

---

## 1. プロジェクト概要

DELILA2は、核物理実験用の高性能データ収集システムです。以下の2つの主要コンポーネントで構成されています：

- **Digitizerライブラリ**: ハードウェアインターフェースとデータ収集
- **Networkライブラリ**: 高性能データ転送とシリアライゼーション

### 主な特徴
- C++17で実装された高性能システム
- ZeroMQを使用した分散処理対応
- リアルタイムモニタリング機能
- データ圧縮とチェックサム検証
- メモリプール最適化による高速処理

---

## 2. 環境構築

### 必要な依存関係

#### 必須
- CMake (3.15以上)
- C++17対応コンパイラ (GCC 7以上、Clang 5以上)
- ZeroMQ (libzmq)
- LZ4圧縮ライブラリ

#### オプション
- ROOT (データ可視化用)
- CAEN FELib (実際のハードウェア使用時)
- Google Test (テスト実行用)

### macOSでのインストール例

```bash
# Homebrewを使用した依存関係のインストール
brew install cmake
brew install zeromq
brew install lz4

# ROOT（オプション）
brew install root
```

### Linux (Ubuntu/Debian)でのインストール例

```bash
# 依存関係のインストール
sudo apt-get update
sudo apt-get install cmake g++ libzmq3-dev liblz4-dev

# ROOT（オプション）
# ROOTの公式サイトから最新版をダウンロード
```

---

## 3. ビルド手順

### 基本的なビルド

```bash
# リポジトリのクローン
git clone https://github.com/yourusername/DELILA2.git
cd DELILA2

# ビルドディレクトリの作成
mkdir build
cd build

# CMakeの設定（デバッグモード）
cmake -DCMAKE_BUILD_TYPE=Debug ..

# ビルド（並列コンパイル）
make -j$(nproc)

# テストの実行（オプション）
ctest --output-on-failure
```

### リリースビルド

```bash
# 最適化されたリリースビルド
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### インストール

```bash
# システムへのインストール（デフォルト: /usr/local）
sudo make install

# カスタムディレクトリへのインストール
cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
make install
```

---

## 4. 基本的な使い方

### 4.1 最小限のサンプルプログラム

```cpp
#include <iostream>
#include <delila/delila.hpp>

using namespace DELILA;

int main() {
    // ライブラリの初期化
    if (!DELILA::initialize()) {
        std::cerr << "DELILA2の初期化に失敗しました" << std::endl;
        return 1;
    }
    
    std::cout << "DELILA2 v" << DELILA::getVersion() 
              << " が正常に初期化されました" << std::endl;
    
    // Digitizerインスタンスの作成
    Digitizer::Digitizer digitizer;
    
    // EventDataの作成と設定
    Digitizer::EventData event;
    event.module = 1;
    event.channel = 0;
    event.timeStampNs = 1234567890;
    event.energy = 2500;
    
    std::cout << "イベントデータを作成しました:" << std::endl;
    std::cout << "  モジュール: " << static_cast<int>(event.module) << std::endl;
    std::cout << "  チャンネル: " << static_cast<int>(event.channel) << std::endl;
    std::cout << "  タイムスタンプ: " << event.timeStampNs << " ns" << std::endl;
    std::cout << "  エネルギー: " << event.energy << std::endl;
    
    // ライブラリのシャットダウン
    DELILA::shutdown();
    
    return 0;
}
```

### 4.2 コンパイル方法

```bash
# DELILA2がインストールされている場合
g++ -std=c++17 sample.cpp -ldelila -lzmq -llz4 -o sample
./sample
```

---

## 5. 実践的なサンプルプログラム

### 5.1 イベントデータの送受信 (Publisher/Subscriber)

#### パブリッシャー側 (データ送信)

```cpp
// pubsub_publisher.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // トランスポートの設定
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://*:5555";    // データ送信用アドレス
    config.data_pattern = "PUB";              // パブリッシャーパターン
    config.bind_data = true;                  // このプロセスがバインド
    
    // ステータスとコマンドアドレスをデータと同じに設定（無効化）
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "設定に失敗しました" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "接続に失敗しました" << std::endl;
        return 1;
    }
    
    std::cout << "パブリッシャーを開始しました。イベント送信中..." << std::endl;
    
    // イベントの連続送信
    uint64_t event_number = 0;
    while (true) {
        // イベントバッチの作成
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        // 10個のイベントを作成
        for (int i = 0; i < 10; ++i) {
            auto event = std::make_unique<EventData>(512);  // 512サンプルの波形
            event->timeStampNs = std::chrono::system_clock::now()
                                    .time_since_epoch().count();
            event->energy = 1000 + (event_number % 2000);
            event->module = 0;
            event->channel = i;
            
            // 波形データの生成（サイン波）
            for (size_t j = 0; j < 512; ++j) {
                event->analogProbe1[j] = static_cast<int32_t>(1000 * sin(j * 0.1));
            }
            
            events->push_back(std::move(event));
            event_number++;
        }
        
        // イベントの送信
        if (!transport.Send(events)) {
            std::cerr << "イベント送信に失敗しました" << std::endl;
        }
        
        // 100ミリ秒待機
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
```

#### サブスクライバー側 (データ受信)

```cpp
// pubsub_subscriber.cpp
#include <iostream>
#include <thread>
#include "ZMQTransport.hpp"
#include "EventData.hpp"

using namespace DELILA::Net;
using namespace DELILA::Digitizer;

int main() {
    // トランスポートの設定
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5555";  // 接続先アドレス
    config.data_pattern = "SUB";                    // サブスクライバーパターン
    config.bind_data = false;                       // このプロセスは接続
    
    // ステータスとコマンドアドレスをデータと同じに設定（無効化）
    config.status_address = config.data_address;
    config.command_address = config.data_address;
    
    if (!transport.Configure(config)) {
        std::cerr << "設定に失敗しました" << std::endl;
        return 1;
    }
    
    if (!transport.Connect()) {
        std::cerr << "接続に失敗しました" << std::endl;
        return 1;
    }
    
    std::cout << "サブスクライバーを開始しました。イベント待機中..." << std::endl;
    
    // 統計情報
    uint64_t total_events = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    // イベントの受信ループ
    while (true) {
        auto [events, timestamp] = transport.Receive();
        
        if (events && !events->empty()) {
            total_events += events->size();
            
            // 最初のイベントの情報を表示
            const auto& first_event = (*events)[0];
            std::cout << "受信: " << events->size() << " イベント, "
                     << "エネルギー: " << first_event->energy << ", "
                     << "チャンネル: " << static_cast<int>(first_event->channel) 
                     << std::endl;
            
            // 1秒ごとに統計を表示
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>
                          (now - start_time).count();
            if (elapsed > 0 && total_events % 100 == 0) {
                double rate = static_cast<double>(total_events) / elapsed;
                std::cout << "合計イベント数: " << total_events 
                         << ", レート: " << rate << " events/s" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

### 5.2 実行方法

#### ターミナル1: パブリッシャーの起動
```bash
cd build/lib/net/examples
./pubsub_publisher
```

#### ターミナル2: サブスクライバーの起動
```bash
cd build/lib/net/examples
./pubsub_subscriber
```

期待される出力:
- パブリッシャー: "イベント送信中..." のメッセージ
- サブスクライバー: 受信したイベントの情報とレート統計

---

## 6. DAQテストシステム

### 6.1 Source/Sinkアーキテクチャ

DAQテストシステムは、実際のデータ収集シナリオをシミュレートします：

- **Source**: デジタイザーからデータを読み取り、ネットワークで配信
- **Sink**: データストリームを受信し、モニタリング/記録を実行

### 6.2 自動テストスクリプトの実行

```bash
# DAQテストディレクトリへ移動
cd daq_test

# 自動テストの実行（10秒間実行）
./test_source_sink.sh
```

このスクリプトは自動的に：
1. Sourceプロセスを起動
2. Sinkプロセスを起動
3. 10秒間データ転送を実行
4. 統計情報を表示
5. プロセスを停止

### 6.3 手動実行（詳細制御）

#### ステップ1: Sourceの起動（ターミナル1）

```bash
cd daq_test

# ROOT環境の設定（必要な場合）
source /opt/ROOT/bin/thisroot.sh

# Sourceの起動
./build/bin/delila_source PSD2.conf "tcp://*:5555"
```

起動時のオプション:
- `PSD2.conf`: デジタイザー設定ファイル
- `tcp://*:5555`: バインドアドレス
- `--compress`: 圧縮を有効化（オプション）
- `--checksum`: チェックサムを有効化（オプション）

期待される出力:
```
デジタイザーを初期化中...
トランスポート接続完了、データ収集開始...
イベントレート: 1000 events/s
```

#### ステップ2: Sinkの起動（ターミナル2）

```bash
cd daq_test

# ROOT環境の設定（必要な場合）
source /opt/ROOT/bin/thisroot.sh

# Sinkの起動（基本）
./build/bin/delila_sink --address tcp://localhost:5555

# 全オプション付きの例
./build/bin/delila_sink \
    --address tcp://localhost:5555 \
    --output-prefix experiment_001 \
    --output-dir ./data \
    --compress \
    --checksum
```

Sinkのオプション:
- `--address`: 接続先アドレス（デフォルト: tcp://localhost:5555）
- `--no-monitor`: モニタリングを無効化
- `--no-recorder`: データ記録を無効化
- `--compress`: 圧縮を有効化（Sourceと一致させる）
- `--checksum`: チェックサム検証を有効化
- `--output-prefix`: 出力ファイルのプレフィックス
- `--output-dir`: 出力ディレクトリ

期待される出力:
```
トランスポート接続完了、データ待機中...
受信イベント数: 10000
レート: 1000 events/s
Webモニター: http://localhost:8080
```

### 6.4 複数Sinkの実行

DELILA2は1つのSourceから複数のSinkへのデータ配信をサポートします：

```bash
# ターミナル1: Source
./build/bin/delila_source PSD2.conf "tcp://*:5555"

# ターミナル2: モニタリング専用Sink
./build/bin/delila_sink --address tcp://localhost:5555 --no-recorder

# ターミナル3: 記録専用Sink
./build/bin/delila_sink --address tcp://localhost:5555 --no-monitor \
    --output-prefix run_001

# ターミナル4: バックアップSink（別マシンから）
./build/bin/delila_sink --address tcp://192.168.1.100:5555 \
    --output-prefix backup_001
```

### 6.5 パフォーマンステスト

#### バッチスケーリングテスト

```bash
cd daq_test
./test_batch_scaling.sh
```

このテストは異なるバッチサイズでのパフォーマンスを測定します。

#### エミュレーターテスト

```bash
cd daq_test
./test_emulator.sh
```

実際のハードウェアなしでデータ収集をシミュレートします。

---

## 7. ネットワークライブラリのサンプル

### 7.1 ロードバランシング (Push/Pull)

複数のワーカーに作業を分散させる例：

#### Pusher（作業配布側）

```bash
cd build/lib/net/examples
./push_pull_pusher
```

#### Puller（ワーカー側）- 複数起動

```bash
# ターミナル1
./push_pull_puller 1

# ターミナル2
./push_pull_puller 2

# ターミナル3
./push_pull_puller 3
```

各ワーカーが自動的に作業を分担します。

### 7.2 コマンド/コントロール

REQ/REPパターンを使用したコマンド送受信：

#### サーバー側
```bash
./command_server
```

#### クライアント側
```bash
./command_client
```

対話的にコマンドを送信し、レスポンスを受信できます。

### 7.3 ステータスモニタリング

システム全体のステータスを監視：

#### レポーター側（複数起動可能）
```bash
./status_reporter
```

#### モニター側
```bash
./status_monitor
```

リアルタイムで各コンポーネントのステータスを表示します。

### 7.4 パフォーマンス測定

#### スループットテスト

```bash
cd build/lib/net/examples
./throughput_test
```

出力例:
```
データスループット測定中...
送信: 1000000 events
経過時間: 2.5 秒
スループット: 400000 events/秒
帯域幅: 1.2 GB/秒
```

#### メモリプール比較

```bash
./memory_pool_example
```

メモリプールあり/なしでのパフォーマンス差を確認できます。

### 7.5 エラーハンドリングとリカバリー

```bash
# 堅牢なクライアントの例
./robust_client
```

このサンプルは以下を実演します：
- 自動再接続
- タイムアウト処理
- エラーログ記録
- グレースフルシャットダウン

---

## 8. トラブルシューティング

### 8.1 よくある問題と解決方法

#### 問題: "Failed to connect" エラー

解決方法:
```bash
# ポートが使用中か確認
lsof -i :5555

# 別のポートを使用
./pubsub_publisher --port 5556
./pubsub_subscriber --address tcp://localhost:5556
```

#### 問題: データが受信されない

チェックリスト:
1. ファイアウォール設定を確認
2. Source側が先に起動しているか確認
3. アドレスとポートが一致しているか確認
4. 圧縮設定が両側で一致しているか確認

```bash
# ネットワーク接続のテスト
nc -zv localhost 5555

# ZeroMQのバージョン確認
pkg-config --modversion libzmq
```

#### 問題: パフォーマンスが低い

最適化方法:
```bash
# 大きなバッチサイズを使用
./delila_source --batch-size 1000

# 圧縮を無効化（LANの場合）
./delila_source --no-compress

# TCPバッファサイズの調整（Linux）
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.wmem_max=134217728
```

### 8.2 デバッグ方法

#### 詳細ログの有効化

```bash
# 環境変数でログレベルを設定
export DELILA_LOG_LEVEL=DEBUG
./delila_source PSD2.conf "tcp://*:5555"
```

#### ネットワークトラフィックの監視

```bash
# tcpdumpを使用
sudo tcpdump -i lo -n port 5555

# Wiresharkでの解析
sudo wireshark -i lo -f "port 5555"
```

#### メモリリークのチェック

```bash
# valgrindを使用（Linux）
valgrind --leak-check=full ./pubsub_publisher

# macOSの場合
leaks --atExit -- ./pubsub_publisher
```

### 8.3 パフォーマンスチューニング

#### システム設定の最適化

```bash
# Linux: カーネルパラメータの調整
sudo sysctl -w net.ipv4.tcp_nodelay=1
sudo sysctl -w net.ipv4.tcp_quickack=1

# ulimitの調整
ulimit -n 65536  # ファイルディスクリプタ数
ulimit -l unlimited  # メモリロック
```

#### コンパイル最適化

```bash
# 最大最適化でビルド
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native" ..
make -j$(nproc)
```

### 8.4 よく使うコマンドリファレンス

```bash
# プロセスの確認
ps aux | grep delila

# ポートの使用状況
netstat -an | grep 5555

# CPUプロファイリング（Linux）
perf record -g ./throughput_test
perf report

# リアルタイムモニタリング
htop -p $(pgrep delila)

# ログのリアルタイム監視
tail -f delila.log | grep ERROR
```

---

## 付録A: 設定ファイルの例

### A.1 デジタイザー設定 (PSD2.conf)

```ini
[Digitizer]
ConnectionType = USB
LinkNum = 0
ConetNode = 0

[Acquisition]
RecordLength = 512
PreTrigger = 100
DCOffset = 0x8000
TriggerThreshold = 100

[Channels]
EnableMask = 0xFF
PulsePolarity = POSITIVE
```

### A.2 トランスポート設定 (transport.json)

```json
{
    "data": {
        "address": "tcp://localhost:5555",
        "pattern": "PUB",
        "bind": true
    },
    "status": {
        "address": "tcp://localhost:5556",
        "pattern": "PUB",
        "bind": true
    },
    "command": {
        "address": "tcp://localhost:5557",
        "pattern": "REP",
        "bind": true
    },
    "options": {
        "compression": true,
        "checksum": true,
        "memory_pool_size": 1000
    }
}
```

---

## 付録B: API リファレンス（主要クラス）

### EventData クラス

```cpp
class EventData {
public:
    uint8_t module;           // モジュール番号
    uint8_t channel;          // チャンネル番号
    uint64_t timeStampNs;     // タイムスタンプ（ナノ秒）
    uint32_t energy;          // エネルギー値
    uint32_t energyShort;     // 短時間ゲートエネルギー
    std::vector<int32_t> analogProbe1;  // 波形データ
    
    EventData(size_t waveformSize = 0);
    size_t GetSerializedSize() const;
};
```

### ZMQTransport クラス

```cpp
class ZMQTransport {
public:
    bool Configure(const TransportConfig& config);
    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    
    // データ送受信
    bool Send(const std::unique_ptr<std::vector<
              std::unique_ptr<EventData>>>& events);
    std::pair<std::unique_ptr<std::vector<
              std::unique_ptr<EventData>>>, uint64_t> Receive();
    
    // ステータス送受信
    bool SendStatus(const ComponentStatus& status);
    std::unique_ptr<ComponentStatus> ReceiveStatus();
    
    // コマンド送受信
    CommandResponse SendCommand(const Command& command);
    std::unique_ptr<Command> ReceiveCommand();
};
```

### TransportConfig 構造体

```cpp
struct TransportConfig {
    std::string data_address;     // データチャンネルアドレス
    std::string data_pattern;     // ZMQパターン (PUB/SUB/PUSH/PULL)
    bool bind_data;               // バインド/接続の選択
    
    std::string status_address;   // ステータスチャンネルアドレス
    std::string command_address;  // コマンドチャンネルアドレス
    
    bool enable_compression;      // 圧縮有効化
    bool enable_checksum;        // チェックサム有効化
    size_t memory_pool_size;     // メモリプールサイズ
};
```

---

## 付録C: 開発者向け情報

### C.1 プロジェクト構造

```
DELILA2/
├── CMakeLists.txt           # メインCMakeファイル
├── include/                 # パブリックヘッダー
│   └── delila/
│       ├── delila.hpp       # アンブレラヘッダー
│       └── version.hpp      # バージョン情報
├── lib/
│   ├── digitizer/          # デジタイザーライブラリ
│   │   ├── include/
│   │   ├── src/
│   │   └── tests/
│   └── net/                # ネットワークライブラリ
│       ├── include/
│       ├── src/
│       ├── examples/       # ネットワークサンプル
│       └── tests/
├── daq_test/              # DAQテストプログラム
│   ├── source.cpp
│   ├── sink.cpp
│   └── test_*.sh          # テストスクリプト
├── examples/              # 基本サンプル
│   └── basic_example.cpp
└── tests/                 # ユニットテスト
    ├── unit/
    ├── integration/
    └── benchmarks/
```

### C.2 新機能の追加方法

1. **ヘッダーファイルの追加**
   ```bash
   # 適切なincludeディレクトリに配置
   lib/net/include/NewFeature.hpp
   ```

2. **実装ファイルの追加**
   ```bash
   lib/net/src/NewFeature.cpp
   ```

3. **CMakeLists.txtの更新**
   ```cmake
   # lib/net/CMakeLists.txt に追加
   set(NET_SOURCES
       ${NET_SOURCES}
       src/NewFeature.cpp
   )
   ```

4. **テストの作成**
   ```bash
   lib/net/tests/unit/test_new_feature.cpp
   ```

5. **サンプルコードの作成**
   ```bash
   lib/net/examples/new_feature_example.cpp
   ```

### C.3 テストの実行

```bash
# 全テストの実行
cd build
ctest --output-on-failure

# 特定のテストのみ
ctest -R ZMQTransport -V

# メモリチェック付き
ctest -T memcheck

# カバレッジレポート（設定済みの場合）
make coverage
```

### C.4 コントリビューション

プルリクエストを送る前に：

1. **コードフォーマット**
   ```bash
   clang-format -i src/*.cpp include/*.hpp
   ```

2. **静的解析**
   ```bash
   clang-tidy src/*.cpp -- -I../include
   ```

3. **テストの追加**
   - 新機能には必ずテストを追加
   - 既存のテストが全てパスすることを確認

4. **ドキュメントの更新**
   - このマニュアルへの追記
   - APIドキュメントのコメント更新

---

## 終わりに

このマニュアルは、DELILA2プロジェクトの基本的な使い方から高度な機能まで幅広くカバーしています。更なる詳細や最新情報については、プロジェクトのGitHubリポジトリやWikiを参照してください。

質問や問題がある場合は、GitHubのIssueトラッカーで報告してください。

Happy Data Acquisition! 🚀