[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)

# 🌱 Embedded Tamagotchi Engine

コンテンツパックを切り替えられる、組み込み機器向けの **C++17 バーチャルペットエンジン**です。現在は **ESP32-S3** 用の実装を提供しています。

形態、食べ物、成長ルール、画像リソースを定義して、共通のエンジン・セーブ機構・演出フローで自分だけのペットを作れます。[Artoria Tamagotchi](https://github.com/fluphus/esp32-artoria-tamagotchi) から発展したプロジェクトで、元のアルトリアのゲームプレイと、独立したオリジナル例 **Pocket Sprout** を収録しています。

## ✨ 機能

- **共通エンジン：** コマンド、時間経過、状態変化、意味的イベント、セーブのスケジューリング。カレンダー時刻の秒と単調増加するミリ秒を区別し、時計と乱数源を注入して決定的にテストできます。
- **コンテンツパック：** 形態・食べ物の安定ID、生成データテーブル、バージョン付き専用状態、特殊ルールを実装する型付きC++ポリシー。パックはコンパイル時に選択します。
- **操作と演出：** 餌やり、つつく、成長、図鑑、リセット確認。容量を制限したアニメーションキュー、入力制限、画面保持により、ルールの適用とアニメーション再生を分離します。
- **永続化：** 3物理スロット・2スロット交互保存、明示的なバージョン付きエンコード、チェックサム、読み戻し検証。アルトリアv9の移行、完全バックアップ、ペットのみのビジットに対応。保護対象の複数キー操作が中断されると復旧状態に入ります。
- **2種類の表示：** **Adafruit GFX / Adafruit SSD1351** によるSSD1351 128×128 OLEDと、シリアルのプレースホルダー表示。PNGをファームウェアに組み込め、未用意の画像にはプレースホルダーを使用します。
- **ホスト回帰テスト：** 周辺機器を差し替え、実際のルール・メニュー・ワークフロー・ストレージ・プロトコルを検証。障害注入、再起動、処理中断も対象です。

## 📦 同梱コンテンツパック

| パック | ゲームプレイ | ビルド選択 |
|---|---|---|
| **Artoria（アルトリア）** — 既定 | セイバー・リリィから始まり、白・黒の成長ルート、食事時間帯、コンボ、スペシャルフード、Rhongomyniad・Nobuルートを含みます。 | `esp32-s3-devkitc-1` / `artoria-serial` |
| **Pocket Sprout** | Seed → Bloom。4種類の食べ物で水を蓄え、年齢1日以上かつ水量6以上で成長します。Dew + Sunseed + Moonberryでスペシャルフード選択が発生し、Pebbleを含む組み合わせではコンボが発生しません。 | `sprout-ssd1351` / `sprout-serial` |

両パックは同じエンジンと永続化機構を使用します。Sproutは独自の成長ポリシーと安定IDを持ち、エンジンのコピーや名前だけを変えた静的デモではありません。

## 🧩 アーキテクチャと対応プラットフォーム

```text
アプリケーションの組み立て
├── エンジン              標準C++17の契約と共通機構
├── コンテンツパック      定義・ポリシー・バージョン付き専用状態
├── プラットフォーム層    時計・乱数・ストレージ・機器ライフサイクル
└── 入力 / 演出 / 表示
```

`src/engine/` は標準ライブラリと自身の契約だけに依存します。現在の完成したファームウェアは **ESP32-S3 / Arduino** 向けで、NVS、ディープスリープ、GPIO、画面との連携を含みます。ホスト実行はテスト用です。他のボードへの移植にはプラットフォーム層とアプリケーションの接続が必要です。実行時のパック読み込みやスクリプトVMはありません。

詳細は[アーキテクチャ](docs/engine/architecture.md)と[パック追加ガイド](docs/engine/add-content-pack.md)を参照してください。現在のアプリケーションの上限は、形態32、通常食4–32、スペシャルフード1–32、安定ID 0–254、専用状態64バイトです。オフライン補償は最大366日で、年齢とタイムスタンプの容量制限も適用されます。

## 🛠️ ハードウェア

同梱設定は **ESP32-S3 N16R8** を対象とします。16 MBフラッシュ、8 MB PSRAM、SSD1351 1.5インチ128×128カラーOLED、3ボタンを使用します。各ボタンは押すと対応GPIOをGNDに接続します。

| 機能 | GPIO |
|---|---:|
| 左 / 中 / 右ボタン | 4 / 5 / 6 |
| OLED DC / リセット / CS | 8 / 9 / 10 |
| OLED MOSI / SCLK | 11 / 12 |

シリアルのプレースホルダーバックエンドもESP32-S3上で動作し、画面を接続せずに使えます。ネイティブC++のホストテストとは別の実行経路です。

## 🚀 ビルドと実行

PlatformIOを使えるPython環境を用意します。ホストテストにはC++17コンパイラ（既定は`g++`）とGitも必要です。以下はリポジトリのルートで実行してください。

```sh
python -m venv .venv
# Linux / macOS
. .venv/bin/activate
# Windows PowerShell: .venv\Scripts\Activate.ps1
python -m pip install platformio==6.2.0 Pillow==12.3.0 pyserial==3.5
```

| 環境 | パック | 表示 |
|---|---|---|
| `esp32-s3-devkitc-1` — 既定 | Artoria | SSD1351 |
| `artoria-serial` | Artoria | シリアル・プレースホルダー |
| `sprout-ssd1351` | Pocket Sprout | SSD1351 |
| `sprout-serial` | Pocket Sprout | シリアル・プレースホルダー |

```sh
pio run                                      # 既定のArtoria OLEDビルド
pio run -e sprout-ssd1351                     # Sprout OLEDビルド
pio run -e artoria-serial -e sprout-serial     # シリアル版ビルド

# 選択した環境をボードに書き込み、コンソールを開きます。
pio run -e esp32-s3-devkitc-1 -t upload
pio device monitor -b 115200
```

初回起動または電源断後は、ボタンで日時を確認します。シリアルの`SET_TIME <unix_seconds>`でも時刻設定を完了できます。現在はソフトウェア時計とRTCを使ったディープスリープからの時刻復元を使用し、ネットワーク時刻同期は実装していません。

セーブはコンテンツパックに紐づきます。パックを変更する前にデータをエクスポートしてください。他パックや未知形式のデータは保護され、新規ゲームで自動的に上書きされません。

## 🎮 操作

| 画面 | L | M | R |
|---|---|---|---|
| 待機 | 餌やり開始 | ステータス；長押しで図鑑 | つつく |
| 食べ物選択 | 前の食べ物 | 選択切り替え；3つで自動確定 | 次の食べ物 |
| スペシャルフード | 前の選択肢 | 確定 | 次の選択肢 |
| 図鑑 | 前の項目 | 閉じる | 次の項目 |
| リセット確認 | YES | 確定 | NO |
| 日時設定 | 現在の項目を増やす | 次の項目 / 確定 | 前の項目 |

ステータスはMで閉じます。**L + M + Rを同時に5秒間**押すとリセット確認を開き、初期選択はNOです。日時設定中はL長押しで編集日時を既定値に戻します。アニメーションや結果画面の保持中はゲーム操作を制限し、スペシャルフード選択では入力を待ちます。

115200ボーのシリアルで操作を試せます。

```text
s                    現在の状態
h                    利用可能な全コマンド
fl                   食べ物一覧
btn l / btn m / btn r 短押しを模擬
btnl m               中ボタン長押しを模擬
ctx                  UIと演出の状態
save / load          現在のペットを保存 / 読み込み
reset                ペットをリセット；ビジット中はオーナーを復元
devinfo              デバイス状態と実行時メモリの観測値
t 60                 デバッグ：分単位のルールを60分進める
d                    デバッグ：1回の日次処理と日付の進行
```

`t`は1–10080分を上限付きバッチで処理し、元の分単位だけを進めるデバッグ仕様を維持します。ビジット中は時刻変更のデバッグコマンドと追加インポートを制限します。初回時刻設定やセーブ復旧を待っている間、ゲームプレイのデバッグ操作はできません。

## 💾 バックアップ・ビジット・復旧

実際のPythonツールを使用します。`/dev/ttyACM0`を自分のポートに置き換えてください（Windowsでは例えば`COM5`）。

```sh
python SaveManager/save_manager.py --port /dev/ttyACM0 status
python SaveManager/save_manager.py --port /dev/ttyACM0 export --slot 0 --out full.bin
python SaveManager/save_manager.py --port /dev/ttyACM0 export --slot 0 --out guest.bin --pet-only
python SaveManager/save_manager.py --port /dev/ttyACM0 import --slot 1 --in full.bin
```

- **完全インポート：** 同梱のペットと図鑑を復元し、ペットのタイムスタンプをローカル時計に合わせます。
- **ペットのみのインポート：** ビジットを開始します。最新のアクティブなオーナースロットを凍結し、ゲストは残り2スロットを使います。オーナースロットへのインポートは拒否されます。`reset`またはリセット確認の確定でビジットを終了し、オーナーを復元します。
- **復旧状態：** 保護対象の操作が未完了の場合、バックグラウンド書き込みとゲーム操作を止めます。まず読めるスロットをエクスポートしてください。オーナーまたはリセット前のペットの副本がある場合、`export --slot -1 --out owner.bin`またはファームウェアの`SAVE_EXPORT_OWNER`で取り出せます。
- **明示的な復旧：** `SAVE_RECOVERY_ERASE CONFIRM`の後、エクスポート済みの完全バックアップを再インポートします。このコマンドは保存データを削除し、復旧ロック中だけ受け付けます。通常の起動では自動削除しません。

シリアルインポートは`SAVE_IMPORT_BEGIN`、`SAVE_IMPORT_DATA`、`SAVE_IMPORT_COMMIT`、`SAVE_IMPORT_ABORT`を使用し、ツールが分割送信と応答確認を処理します。未知のバージョン・パック・IDは拒否されます。現在のv10セーブは明示的フィールドエンコードです。旧アルトリアv9の移行証拠は検証報告に記録しています。

## 🎨 リソースと独自コンテンツ

パックの`manifest.json`で形態、食べ物、テキスト、リソース対応を編集し、テーブルを生成します。

```sh
python tools/engine/generate_content.py
python tools/engine/generate_content.py --check

# 任意のPNG変換。Pillowが必要です。
python tools/convert_assets.py --content artoria
python tools/convert_assets.py --content sprout
```

生成済みテーブルとリソースはソースに含まれています。画像変換は手動で、PlatformIOのプリビルドフックは無効です。Artoriaは`assets/`、Sproutは`assets/sprout/`を使用します。画像の整備状況は異なり、Sproutは名前付きプレースホルダーを使用します。

[画像の命名ガイド](assets/README.md)と[パック追加ガイド](docs/engine/add-content-pack.md)を参照してください。新しいパックはデータと型付きC++ポリシーを用意し、アプリケーションにビルドとリソース選択を登録します。エンジンをコピーする必要はありません。

## 🧪 テストと検証

```sh
python tests/host/check_architecture.py  # エンジン依存と独立ポリシーの検証
python tests/host/run_all.py            # 両パックの実際の製品コード経路を回帰検証
python tests/host/run_all.py --resources
```

完全なテストは元のGitベースラインを参照してv9の出所と旧ゲームプレイとの一致を検証します。その履歴を保持するGitチェックアウトを使ってください。ソースZIPには必要な履歴がありません。ライフサイクル、入力と演出、破損セーブ、書き込み失敗、インポート/リセット中断、オーナー復元、時刻境界、実際のPythonプロトコルツールを検証します。ASan/UBSanのコマンドは報告書に記載しています。

- [検証の概要](docs/engine/acceptance.md)
- [最新D1–D6修正と回帰検証の証拠](docs/engine/deep-fixes-2026-10-13.md)
- [D1–D6の独立した再検証](docs/engine/deep-reacceptance-2026-10-13.md)
- [以前の実機テストと対象バージョン](docs/engine/hardware-acceptance.md)
- [元の挙動と維持している時間仕様](docs/engine/behavior-baseline.md)

最新修正はホスト回帰テストと4環境ビルドで検証済みですが、新ファームウェアを書き込んだ実機再検証は未実施です。画面・物理ボタン、実際のスリープ復帰、書き込み中の電源断は未検証項目です。最大オフライン補償は同期処理で、以前のファームウェアでは366日分に約13–15秒かかりました。

## 📄 ライセンスと謝辞

コードは[MITライセンス](LICENSE)です。[Artoria Tamagotchi](https://github.com/fluphus/esp32-artoria-tamagotchi)を基にしています。ArtoriaはFate/Grand Orderを題材にしたサンプル、Pocket Sproutはオリジナルのサンプルコンテンツパックです。
