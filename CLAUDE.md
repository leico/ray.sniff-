# ray.sniff~

ネットワークのパケットを生のバイト列のまま音声信号として出力する Max/MSP の external。libpcap を使う。

**仕様・決定事項・未決定の事項は [docs/design.md](docs/design.md) にまとめている。設計に関わる作業をする前に必ず読むこと。** 新しく決まったことは、決まったその場で design.md に反映する。

## ビルド

```sh
git submodule update --init
cmake -B build && cmake --build build   # → externals/ray.sniff~.mxo（arm64 + x86_64）
lipo -info "externals/ray.sniff~.mxo/Contents/MacOS/ray.sniff~"
```

## 構成

- `source/ray.sniff~/ray.sniff~.cpp`：Max オブジェクト（引数、attribute、DSP、メッセージ）
- `source/ray.sniff~/packet_capture.{h,cpp}`：pcap とキュー（Max API に依存しない）
- `max-sdk-base/`：submodule。編集しない
- `help/`：ヘルプのパッチ

## コーディング規約

- クラスの private / protected なメンバ変数は、**末尾にアンダースコアを付ける**（`count_`）。先頭にアンダースコアを付ける形（`_count`）は使わない（`_` ＋大文字や `__` で始まる名前は C++ の規格で予約されているため）
- 外から直接読み書きするデータだけの構造体（`CaptureConfig` や、Max オブジェクトの `t_raysniff` など）のメンバには付けない

## 守ること

- オーディオスレッド（perform64）では、ロックを取らず、メモリの確保も解放もしない
- `packet_capture` は Max API に依存させない（Max なしでテストプログラムからビルドできるようにしておく）
- キャプチャには `/dev/bpf*` へのアクセス権が必要。権限のない環境では、`PacketQueue` の単体テストと、権限エラー時の案内表示までしか確認できない
- グローバルの gitignore にある `*~` が `ray.sniff~` にマッチする。`git add` したファイルが漏れていないか確認すること
