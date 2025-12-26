# KISS版 リファクタリング計画

## 目的

**最小限の変更**で以下を実現する:
1. LZ4圧縮を削除（不要な依存関係の除去）
2. 共有ドメインオブジェクトを lib/core に分離
3. IComponent インターフェース追加（コンポーネントの雛形）
4. Command通信（REQ/REP）を ZMQTransport に追加

## 現状の問題点

- LZ4圧縮は使用していないが、依存関係として残っている
- `ComponentState` が lib/net にあるが、lib/digitizer でも使いたい
- コンポーネント（Digitizer, Sink等）の共通インターフェースがない
- Command通信（Operator → Digitizer）が未実装

## 方針: KISS + YAGNI

- **必要なインターフェースのみ追加**（IComponent）
- **既存の ZMQTransport/DataProcessor はそのまま拡張**
- **テストは既存を維持、必要な分だけ追加**
- 不要なもの（LZ4）は削除

---

## タスク一覧

### Step 1: LZ4 圧縮削除

DataProcessor から LZ4 圧縮機能を削除。

**変更ファイル**:
- `lib/net/src/DataProcessor.cpp` - LZ4関連コード削除
- `lib/net/include/DataProcessor.hpp` - compression_enabled_ 削除
- `lib/net/CMakeLists.txt` - lz4 依存削除
- `CMakeLists.txt` - lz4 依存削除
- 関連ドキュメント更新

**作業内容**:
- [ ] DataProcessor から CompressLZ4/DecompressLZ4 削除
- [ ] compression_type を常に COMPRESSION_NONE に
- [ ] CMakeLists.txt から lz4 依存削除
- [ ] テスト修正（圧縮関連テストがあれば）
- [ ] ビルド確認

---

### Step 2: lib/core 作成（共有ドメイン）

lib/net と lib/digitizer で共有するオブジェクトを分離。

```
lib/core/
├── include/delila/core/
│   ├── ComponentState.hpp    # lib/net から移動
│   ├── Command.hpp           # 新規
│   ├── CommandResponse.hpp   # 新規
│   ├── ErrorCode.hpp         # 新規
│   └── IComponent.hpp        # 新規（コンポーネント雛形）
└── CMakeLists.txt
```

**IComponent インターフェース**:
```cpp
// lib/core/include/delila/core/IComponent.hpp
#pragma once

#include "ComponentState.hpp"
#include <string>

namespace DELILA {

class IComponent {
public:
    virtual ~IComponent() = default;

    // ライフサイクル
    virtual bool Configure(const std::string& config_path) = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual void Reset() = 0;

    // 状態
    virtual ComponentState GetState() const = 0;
    virtual std::string GetComponentId() const = 0;
};

}  // namespace DELILA
```

**作業内容**:
- [ ] ディレクトリ作成
- [ ] ComponentState.hpp を lib/net から移動
- [ ] Command, CommandResponse, ErrorCode を新規作成
- [ ] IComponent.hpp を新規作成
- [ ] CMakeLists.txt 作成（header-only）
- [ ] lib/net の include を更新
- [ ] ビルド確認

---

### Step 3: ZMQTransport に Command 通信追加

既存の ZMQTransport クラスに REQ/REP 機能を追加。

**変更ファイル**:
- `lib/net/include/ZMQTransport.hpp`
- `lib/net/src/ZMQTransport.cpp`

**追加するメンバ**:
```cpp
// TransportConfig に追加
std::string command_address;  // REQ/REP用アドレス
bool bind_command = true;     // true=REP(サーバー), false=REQ(クライアント)

// ZMQTransport に追加
std::unique_ptr<zmq::socket_t> fCommandSocket;

// メソッド追加（Operator側 - REQ）
std::optional<CommandResponse> SendCommand(const Command& cmd,
                                           std::chrono::milliseconds timeout);

// メソッド追加（Component側 - REP）
std::optional<Command> ReceiveCommand(std::chrono::milliseconds timeout);
bool SendCommandResponse(const CommandResponse& resp);
```

**作業内容**:
- [ ] TransportConfig に command_address, bind_command 追加
- [ ] ZMQTransport に fCommandSocket 追加
- [ ] Configure() で command socket 作成
- [ ] Connect() で command socket 接続
- [ ] SendCommand / ReceiveCommand / SendCommandResponse 実装
- [ ] JSON シリアライズ（nlohmann/json 使用）
- [ ] ビルド確認

---

### Step 4: テスト追加

**新規テストファイル**: `tests/unit/net/test_command_channel.cpp`

```cpp
TEST(CommandChannelTest, SendReceiveCommand) {
    // REP側（Component/サーバー）
    ZMQTransport server;
    TransportConfig server_config;
    server_config.command_address = "tcp://127.0.0.1:5560";
    server_config.bind_command = true;
    server.Configure(server_config);
    server.Connect();

    // REQ側（Operator/クライアント）
    ZMQTransport client;
    TransportConfig client_config;
    client_config.command_address = "tcp://127.0.0.1:5560";
    client_config.bind_command = false;
    client.Configure(client_config);
    client.Connect();

    // サーバー側スレッド
    auto server_thread = std::async([&]() {
        auto cmd = server.ReceiveCommand(std::chrono::seconds(5));
        EXPECT_TRUE(cmd.has_value());
        EXPECT_EQ(cmd->type, CommandType::ARM);

        CommandResponse resp;
        resp.request_id = cmd->request_id;
        resp.success = true;
        resp.current_state = ComponentState::Armed;
        return server.SendCommandResponse(resp);
    });

    // クライアント側
    Command cmd;
    cmd.type = CommandType::ARM;
    cmd.request_id = 123;

    auto response = client.SendCommand(cmd, std::chrono::seconds(5));
    EXPECT_TRUE(response.has_value());
    EXPECT_TRUE(response->success);
    EXPECT_EQ(response->current_state, ComponentState::Armed);

    EXPECT_TRUE(server_thread.get());
}
```

**作業内容**:
- [ ] test_command_channel.cpp 作成
- [ ] CMakeLists.txt にテスト追加
- [ ] テスト実行・確認

---

## 完了条件

- [ ] LZ4依存が削除されている
- [ ] lib/core が作成され、ビルドが通る
- [ ] IComponent インターフェースが定義されている
- [ ] ZMQTransport で Command 送受信ができる
- [ ] 既存のテストがすべてパス
- [ ] 新規テストがパス

---

## 今回やらないこと（YAGNI）

- IDataChannel, IStatusChannel, ISerializer 等の細かいインターフェース
- 複雑なレイヤー構造（domain, infrastructure, application, facade）
- DataProcessor の分割
- MetricsCollector, StateManager 等の新規クラス

これらは**必要になった時に**追加する。

---

## 実装順序

```
Step 1 (LZ4削除) → Step 2 (lib/core) → Step 3 (Command通信) → Step 4 (テスト)
```

各ステップ完了後にコミットし、動作確認を行う。
