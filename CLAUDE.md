# ray.sniff~

ネットワークのパケットを生のバイト列のまま音声信号として出力する Max/MSP の external。libpcap を使う。

**仕様・決定事項・未決定の事項は [docs/design.md](docs/design.md) にまとめている。設計に関わる作業をする前に必ず読むこと。** 新しく決まったことは、決まったその場で design.md に反映する。

`std::atomic`、memory order、`alignas` の解説は [docs/atomic.md](docs/atomic.md) にある（人が読むための資料）。キューの memory order を変えたときは、atomic.md の「このプロジェクトでの使い方一覧」も合わせて直す。

## ビルド

```sh
git submodule update --init
cmake -B build && cmake --build build   # → externals/ray.sniff~.mxo（arm64 + x86_64）
lipo -info "externals/ray.sniff~.mxo/Contents/MacOS/ray.sniff~"
```

## テスト（Catch2）

```sh
# 通常
cmake -B build-test -DRAYSNIFF_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-test -j && ctest --test-dir build-test --output-on-failure

# ThreadSanitizer（lock-free キューの変更後は必ず実行する）。ASan は thread を address に変える
cmake -B build-thread -DRAYSNIFF_BUILD_TESTS=ON -DRAYSNIFF_SANITIZER=thread -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-thread -j --target raysniff_tests && ./build-thread/tests/raysniff_tests
```

TSan と ASan は同時に使えないので、ビルドディレクトリを分ける。

## 構成

- `source/core/`：Max に依存しない部分（静的ライブラリ `raysniff_core`）。external とテストの両方がリンクする
  - `locked_memory`：`mmap` と `mlock` で確保する記憶領域
  - `byte_ring`：lock-free SPSC のリングバッファ（`produced_` / `consumed_`）
  - `packet_queue`：リングバッファと stats のカウンタ、`@queuesize` の丸め処理
- `source/ray.sniff~/ray.sniff~.cpp`：Max オブジェクト（引数、attribute、DSP、メッセージ）
- `source/ray.sniff~/packet_capture.{h,cpp}`：**前の段階のコード**。手順 4 で `source/core/` に作り直す
- `tests/`：Catch2 の単体テスト
- `max-sdk-base/`：submodule。編集しない
- `help/`：ヘルプのパッチ

## コーディング規約

- クラスの private / protected なメンバ変数は、**末尾にアンダースコアを付ける**（`count_`）。先頭にアンダースコアを付ける形（`_count`）は使わない（`_` ＋大文字や `__` で始まる名前は C++ の規格で予約されているため）
- 外から直接読み書きするデータだけの構造体（`CaptureConfig` や、Max オブジェクトの `t_raysniff` など）のメンバには付けない
- 定数（`constexpr` / `static const`）は、**`k` ＋大文字始まり**にする（`kCacheLine`）。全部大文字（`CACHE_LINE`）はマクロ（`#define`）と見分けが付かなくなるので使わない

## 守ること

- オーディオスレッド（perform64）では、ロックを取らず、メモリの確保も解放もしない
- `source/core/` は Max API に依存させない（Max なしでテストからビルドできるようにしておく）
- キャプチャには `/dev/bpf*` へのアクセス権が必要。権限のない環境では、実際のデバイスからのキャプチャは確認できない（`.pcap` ファイルを使ったテストで代わりに確認する）
- グローバルの gitignore にある `*~` が `ray.sniff~` にマッチする。`git add` したファイルが漏れていないか確認すること
