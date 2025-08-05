# DELILA2 ネットワークライブラリ詳細ドキュメント

## 概要

DELILA2ネットワークライブラリは、核物理学実験のデータ取得システム（DAQ）向けに設計された高性能データ転送ライブラリです。ZeroMQをベースとした堅牢な通信層と、効率的なバイナリシリアライゼーションを提供します。

## アーキテクチャ

### 主要コンポーネント

```
┌─────────────────────────────────────────────────────────────┐
│                    アプリケーション層                          │
├─────────────────────────────────────────────────────────────┤
│                    ZMQTransport クラス                       │
│  ┌─────────────┬──────────────┬────────────────┐          │
│  │ データチャネル │ ステータスチャネル │ コマンドチャネル │          │
│  │  (PUB/SUB)  │   (REQ/REP)   │   (REQ/REP)   │          │
│  └─────────────┴──────────────┴────────────────┘          │
├─────────────────────────────────────────────────────────────┤
│                    Serializer クラス                         │
│  ┌─────────────┬──────────────┬────────────────┐          │
│  │ バイナリ形式  │  LZ4圧縮      │ xxHashチェックサム │          │
│  └─────────────┴──────────────┴────────────────┘          │
├─────────────────────────────────────────────────────────────┤
│                      ZeroMQ ライブラリ                        │
└─────────────────────────────────────────────────────────────┘
```

### 名前空間構造

```cpp
namespace DELILA {
    namespace Net {
        // ネットワーク関連クラス
        class Serializer;
        class ZMQTransport;
        
        // 設定構造体
        struct TransportConfig;
        struct ComponentStatus;
        struct Command;
        struct CommandResponse;
    }
}
```

## Serializer クラス

### 機能概要

Serializerクラスは、EventDataの高速バイナリシリアライゼーションを提供します。

#### 主な特徴
- **バイナリ形式**: 効率的なメモリレイアウト
- **LZ4圧縮**: 高速圧縮・展開（オプション）
- **xxHashチェックサム**: データ整合性検証（オプション）
- **バッファプール**: メモリ割り当てオーバーヘッドの削減

### バイナリフォーマット

```cpp
struct BinaryDataHeader {
    uint32_t magic_number;      // 0xDEADBEEF（フォーマット識別子）
    uint64_t sequence_number;   // シーケンス番号
    uint32_t format_version;    // フォーマットバージョン
    uint32_t header_size;       // ヘッダーサイズ
    uint32_t event_count;       // イベント数
    uint64_t uncompressed_size; // 非圧縮時サイズ
    uint64_t compressed_size;   // 圧縮後サイズ
    uint64_t checksum;          // xxHashチェックサム
    uint64_t timestamp;         // タイムスタンプ
    uint64_t reserved[5];       // 将来の拡張用
};
```

### 使用方法

```cpp
// シリアライザの初期化
Serializer serializer;
serializer.EnableCompression(true);  // LZ4圧縮を有効化
serializer.EnableChecksum(true);     // チェックサムを有効化

// EventDataのエンコード
auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
// ... eventsにデータを追加 ...

uint64_t sequenceNumber = 1;
auto encodedData = serializer.Encode(events, sequenceNumber);

// データのデコード
auto [decodedEvents, decodedSeqNum] = serializer.Decode(encodedData);
```

### パフォーマンス最適化

#### バッファプール機能
```cpp
serializer.EnableBufferPool(true);    // バッファプールを有効化
serializer.SetBufferPoolSize(100);    // プールサイズを設定
size_t pooledCount = serializer.GetPooledBufferCount();  // 現在のプール使用状況
```

## ZMQTransport クラス

### 機能概要

ZMQTransportクラスは、ZeroMQを使用した3つの通信チャネルを提供します。

#### 通信チャネル

1. **データチャネル**: 高スループットのイベントデータ転送
2. **ステータスチャネル**: コンポーネント状態の監視
3. **コマンドチャネル**: DAQシステムの制御コマンド

### 設定構造体

#### TransportConfig
```cpp
struct TransportConfig {
    // アドレス設定
    std::string data_address = "tcp://localhost:5555";
    std::string status_address = "tcp://localhost:5556";
    std::string command_address = "tcp://localhost:5557";
    
    // ソケット設定
    bool bind_data = true;      // true: bind, false: connect
    bool bind_status = true;
    bool bind_command = false;
    
    // 通信パターン設定
    std::string data_pattern = "PUB";  // PUB/SUB/PUSH/PULL
    bool is_publisher = true;          // 役割設定
    
    // サーバーモード設定
    bool server_mode = false;
    bool status_server = false;
    bool command_server = false;
};
```

#### ComponentStatus
```cpp
struct ComponentStatus {
    std::string component_id;                    // コンポーネントID
    std::string state;                          // 状態（IDLE/RUNNING/ERROR等）
    uint64_t timestamp;                         // タイムスタンプ
    std::map<std::string, double> metrics;      // パフォーマンスメトリクス
    std::string error_message;                  // エラーメッセージ
    uint64_t heartbeat_counter;                 // ハートビートカウンタ
};
```

#### Command & CommandResponse
```cpp
enum class CommandType {
    START,      // 開始
    STOP,       // 停止
    PAUSE,      // 一時停止
    RESUME,     // 再開
    CONFIGURE,  // 設定
    RESET,      // リセット
    SHUTDOWN    // シャットダウン
};

struct Command {
    std::string command_id;                           // コマンドID
    CommandType type;                                 // コマンドタイプ
    std::string target_component;                     // 対象コンポーネント
    std::map<std::string, std::string> parameters;   // パラメータ
    uint64_t timestamp;                               // タイムスタンプ
};

struct CommandResponse {
    std::string command_id;                           // コマンドID（Commandと一致）
    bool success;                                     // 成功/失敗
    std::string message;                              // メッセージ
    uint64_t timestamp;                               // タイムスタンプ
    std::map<std::string, std::string> result_data;  // 結果データ
};
```

### 設定方法

#### 1. C++構造体による設定（従来方式）
```cpp
TransportConfig config;
config.data_address = "tcp://localhost:5555";
config.is_publisher = true;
config.bind_data = true;

ZMQTransport transport;
transport.Configure(config);
transport.Connect();
```

#### 2. JSONオブジェクトによる設定（推奨）
```cpp
#include <nlohmann/json.hpp>

nlohmann::json config = {
    {"data_address", "tcp://localhost:5555"},
    {"status_address", "tcp://localhost:5556"},
    {"command_address", "tcp://localhost:5557"},
    {"bind_data", true},
    {"is_publisher", true},
    {"compression", true},
    {"checksum", true},
    {"zero_copy", false},
    {"memory_pool_enabled", true},
    {"memory_pool_size", 20}
};

ZMQTransport transport;
transport.ConfigureFromJSON(config);
transport.Connect();
```

#### 3. JSONファイルによる設定
```cpp
// config.jsonファイルから読み込み
ZMQTransport transport;
transport.ConfigureFromFile("config.json");
transport.Connect();
```

### JSON設定スキーマ

```json
{
  "data_address": "tcp://localhost:5555",
  "status_address": "tcp://localhost:5556",
  "command_address": "tcp://localhost:5557",
  "bind_data": true,
  "bind_status": true,
  "bind_command": false,
  "is_publisher": true,
  "server_mode": false,
  "status_server": false,
  "command_server": false,
  "compression": true,
  "checksum": true,
  "zero_copy": false,
  "memory_pool_enabled": true,
  "memory_pool_size": 20,
  "receive_timeout_ms": 1000
}
```

### 使用パターン

#### 1. データ転送（PUB/SUB）

##### パブリッシャー（送信側）
```cpp
// 設定
TransportConfig config;
config.data_address = "tcp://*:5555";
config.is_publisher = true;
config.bind_data = true;

// 初期化
ZMQTransport publisher;
publisher.Configure(config);
publisher.Connect();

// データ送信
auto events = CreateEventData();  // EventDataの作成
publisher.Send(events);
```

##### サブスクライバー（受信側）
```cpp
// 設定
TransportConfig config;
config.data_address = "tcp://localhost:5555";
config.is_publisher = false;
config.bind_data = false;

// 初期化
ZMQTransport subscriber;
subscriber.Configure(config);
subscriber.Connect();

// データ受信
auto [events, sequenceNumber] = subscriber.Receive();
if (events) {
    std::cout << "受信イベント数: " << events->size() << std::endl;
}
```

#### 2. ステータス通信

```cpp
// ステータス送信
ComponentStatus status;
status.component_id = "digitizer_01";
status.state = "RUNNING";
status.heartbeat_counter = 42;
status.metrics["event_rate"] = 1000.0;
status.metrics["buffer_usage"] = 0.75;

transport.SendStatus(status);

// ステータス受信
auto receivedStatus = transport.ReceiveStatus();
if (receivedStatus) {
    std::cout << "コンポーネント: " << receivedStatus->component_id 
              << " 状態: " << receivedStatus->state << std::endl;
}
```

#### 3. コマンド通信

##### コマンド送信（クライアント）
```cpp
// コマンド作成
Command cmd;
cmd.command_id = "cmd_001";
cmd.type = CommandType::START;
cmd.target_component = "digitizer_01";
cmd.parameters["acquisition_time"] = "60";

// コマンド送信と応答待機
auto response = transport.SendCommand(cmd);
if (response.success) {
    std::cout << "コマンド成功: " << response.message << std::endl;
} else {
    std::cerr << "コマンド失敗: " << response.message << std::endl;
}
```

##### コマンド受信（サーバー）
```cpp
// サーバーモードで起動
transport.StartServer();

// コマンド待機
auto command = transport.WaitForCommand(5000);  // 5秒タイムアウト
if (command) {
    // コマンド処理
    CommandResponse response;
    response.command_id = command->command_id;
    response.success = true;
    response.message = "コマンドを正常に実行しました";
    
    // 応答送信
    transport.SendCommandResponse(response);
}
```

### パフォーマンス機能

#### 1. ゼロコピー最適化
```cpp
transport.EnableZeroCopy(true);
bool isEnabled = transport.IsZeroCopyEnabled();
```

#### 2. メモリプール
```cpp
transport.EnableMemoryPool(true);
transport.SetMemoryPoolSize(50);  // 最大50個のコンテナをプール
size_t poolSize = transport.GetMemoryPoolSize();
size_t pooledCount = transport.GetPooledContainerCount();
```

#### 3. 圧縮とチェックサム
```cpp
transport.EnableCompression(true);   // LZ4圧縮
transport.EnableChecksum(true);      // xxHashチェックサム
```

## エラー処理

### エラー処理パターン

```cpp
// 設定エラー
if (!transport.Configure(config)) {
    std::cerr << "設定エラー: 無効な設定パラメータ" << std::endl;
    return false;
}

// 接続エラー
if (!transport.Connect()) {
    std::cerr << "接続エラー: ZeroMQソケットの初期化に失敗" << std::endl;
    return false;
}

// データ受信エラー（タイムアウト含む）
auto [events, seq] = transport.Receive();
if (!events) {
    std::cerr << "受信エラー: データなしまたはタイムアウト" << std::endl;
}

// コマンド実行エラー
auto response = transport.SendCommand(cmd);
if (!response.success) {
    std::cerr << "コマンドエラー: " << response.message << std::endl;
}
```

## スレッドセーフティ

### スレッドセーフティの考慮事項

1. **Serializer**: 読み取り操作はスレッドセーフ、書き込みは外部同期が必要
2. **ZMQTransport**: スレッドセーフではない、スレッドごとにインスタンスを作成するか外部同期を使用
3. **EventData**: 構築後の読み取りはスレッドセーフ

### マルチスレッド使用例

```cpp
// スレッドごとに独立したトランスポート
void workerThread(int threadId) {
    ZMQTransport transport;
    TransportConfig config;
    config.data_address = "tcp://localhost:5555";
    config.is_publisher = false;
    config.bind_data = false;
    
    transport.Configure(config);
    transport.Connect();
    
    while (running) {
        auto [events, seq] = transport.Receive();
        // イベント処理
    }
}
```

## パフォーマンスベンチマーク

### 測定方法

```cpp
#include <chrono>

// エンコード性能測定
auto start = std::chrono::high_resolution_clock::now();
auto encoded = serializer.Encode(events, sequenceNumber);
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
double throughput = (encoded.size() / 1024.0 / 1024.0) / (duration.count() / 1e6);
std::cout << "エンコード速度: " << throughput << " MB/s" << std::endl;
```

### 期待される性能

- **データスループット**: >100 MB/s（ローカル接続）
- **レイテンシ**: <1ms（コマンド応答）
- **圧縮率**: 30-70%（波形データ依存）
- **CPU使用率**: <10%（1000イベント/秒）

## トラブルシューティング

### よくある問題と解決方法

#### 1. "Address already in use" エラー
```bash
# 使用中のポートを確認
lsof -i :5555

# 解決方法
# - 別のポート番号を使用
# - bind_data = false に変更（クライアント側）
```

#### 2. "Connection refused" エラー
```cpp
// パブリッシャーを先に起動
// ZeroMQの"slow joiner"問題を回避
std::this_thread::sleep_for(std::chrono::milliseconds(100));
```

#### 3. データが受信できない
```cpp
// サブスクライバーの接続待機
transport.Connect();
std::this_thread::sleep_for(std::chrono::milliseconds(50));

// タイムアウト設定の確認
auto [events, seq] = transport.Receive(5000);  // 5秒タイムアウト
```

#### 4. メモリリーク
```cpp
// スマートポインタの正しい使用
auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
// 手動でのメモリ管理は不要
```

## ビルドとテスト

### ビルド手順

```bash
# 依存関係のインストール（macOS）
brew install zeromq lz4 xxhash

# 依存関係のインストール（Ubuntu/Debian）
sudo apt-get install libzmq3-dev liblz4-dev libxxhash-dev

# ビルド
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### テスト実行

```bash
# 全テスト実行
ctest --output-on-failure

# 特定のテスト実行
ctest -R test_zmq_transport -V

# ベンチマーク実行
./examples/throughput_test
```

## サンプルプログラム

### 基本的なPub/Subの例

#### publisher_example.cpp
```cpp
#include "ZMQTransport.hpp"
#include "EventData.hpp"
#include <thread>
#include <random>

int main() {
    using namespace DELILA::Net;
    using namespace DELILA::Digitizer;
    
    // JSON設定
    nlohmann::json config = {
        {"data_address", "tcp://*:5555"},
        {"is_publisher", true},
        {"compression", true},
        {"memory_pool_size", 50}
    };
    
    // トランスポート初期化
    ZMQTransport publisher;
    if (!publisher.ConfigureFromJSON(config)) {
        std::cerr << "設定エラー" << std::endl;
        return 1;
    }
    
    if (!publisher.Connect()) {
        std::cerr << "接続エラー" << std::endl;
        return 1;
    }
    
    // ランダムデータ生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> energy_dist(100, 4000);
    
    uint64_t sequenceNumber = 0;
    
    while (true) {
        // イベントデータ生成
        auto events = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
        
        for (int i = 0; i < 100; ++i) {
            auto event = std::make_unique<EventData>();
            event->timeStampNs = std::chrono::system_clock::now().time_since_epoch().count();
            event->energy = energy_dist(gen);
            event->module = 0;
            event->channel = i % 16;
            
            // 波形データ（サンプル）
            event->analogProbe1.resize(1000);
            for (auto& sample : event->analogProbe1) {
                sample = energy_dist(gen);
            }
            
            events->push_back(std::move(event));
        }
        
        // データ送信
        if (publisher.Send(events)) {
            std::cout << "送信: シーケンス " << sequenceNumber 
                      << ", イベント数 " << events->size() << std::endl;
            sequenceNumber++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
```

## まとめ

DELILA2ネットワークライブラリは、核物理学実験のデータ取得システムに必要な以下の機能を提供します：

1. **高性能**: 100MB/s以上のスループット、低レイテンシ
2. **信頼性**: データ整合性検証、自動再接続
3. **柔軟性**: 複数の通信パターン、JSON設定
4. **拡張性**: 分散システム対応、スケーラブル設計
5. **使いやすさ**: シンプルなAPI、包括的なドキュメント

このライブラリは、KISS原則に従って設計されており、必要最小限の機能から始めて段階的に拡張できる構造になっています。