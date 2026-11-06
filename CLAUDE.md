# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクト概要

OiTR の新世代版を ESP32-P4 ベースの 10.1 インチ表示モジュールで開発するリポジトリ。
最初のマイルストーンは **SD カード上の動画 (`elis.mp4`) を 1280x720 で再生する** こと。

- 開発は **ESP-IDF をコマンドラインのみ** で進める (IDE は使わない)。必要なら PlatformIO 併用可。
- 描画ライブラリは **LovyanGFX** (https://github.com/lovyan03/LovyanGFX) を使う方針。
- 表示レイアウト: 画面は横向き 1280x800。動画を上端に 1280x720 で再生し、**下 80px はステータスバー** にする。

現時点でアプリケーションのソースはまだ無い。`docs/` にベンダー提供資料とデモ一式が置かれている。

## ハードウェア (docs/JC8012P4A1C_I_W_Y より)

ボードは Guition (JCZN) **JC8012P4A1C_I_W_Y**。

| 項目 | 値 |
| --- | --- |
| メイン SoC | ESP32-P4 (RISC-V dual core 400 MHz, 32 MB PSRAM, 16 MB Flash) |
| 無線 | **ESP32-C6** スレーブ (ESP-Hosted 経由, SDIO/SPI/UART)。P4 自体に Wi-Fi/BT は無い |
| パネル | 10.1" IPS, **JD9365** ドライバ, ネイティブ解像度 **800x1280 (縦長)**, MIPI-DSI **2 lane** |
| タッチ | GSL3680 (I2C) |
| オーディオ | ES8311 codec (I2S), PA enable GPIO20 |
| カメラ | OV02C10 (MIPI-CSI) |
| 書き込み | **USB1** ポート (CH340 シリアル)。macOS では `/dev/cu.usbserial-110` として見えている。600 mA 以上の給電が必要 |

主要ピン (ベンダー BSP `esp32_p4_function_ev_board.h` / `pins_config.h` より):

| 機能 | GPIO |
| --- | --- |
| LCD RST / Backlight (LEDC PWM) | 27 / 23 |
| Touch RST / INT | 22 / 21 |
| I2C SDA / SCL (touch, codec, camera SCCB) | 7 / 8 |
| SD (SDMMC 4-bit) D0-D3 / CMD / CLK | 39,40,41,42 / 44 / 43 |
| I2S MCLK / BCLK / WS / DOUT / DIN | 13 / 12 / 10 / 9 / 11 |
| MIPI DSI PHY LDO | channel 3, 2500 mV (必須) |

DPI タイミング (ベンダーデモ): h=800, v=1280, hsync 20/20/40, vsync 4/10/30 (pulse/back/front), DPI clock 60–70 MHz, lane bit rate 1000 Mbps, RGB565。

### 旧パネル / 新パネル

同じ型番で **JD9365 の初期化コマンド列が異なる 2 種類のパネル** が存在する。
背面ラベルの型番末尾に **"V2" が付くものは新パネル**。ベンダーデモは `Old_Panel` / `New_Panel` で分かれている。
初期化シーケンスは `docs/.../xiaozhi-esp32-main/main/boards/guition-jc8012p4a1{,-new}/jc8012p4a1.cc` の `lcd_cmd[]`
または `docs/.../video_lcd_display/main/app_lcd.c` (`CONFIG_BOARD_TYPE_JC8012P4A1` ブロック) からコピーする。
どちらのパネルか不明な場合は両方試し、動いた方を記録すること。

### 解像度についての注意

パネルは物理的に 800x1280 (縦)。横向き 1280x800 で使うには **回転が必要** で、ベンダーの camera→LCD デモは
P4 の PPA (SRM) で 270° 回転してからフレームバッファに書いている。
本プロジェクトは横向き 1280x800 を前提とし、y=0..719 が動画領域、y=720..799 がステータスバー領域。

## 開発環境・コマンド

### ESP-IDF

ローカルの ESP-IDF は `~/esp/esp-idf` で **v6.1** を checkout 済み (ESP32-P4 ツールチェーン導入済み)。更新時は最新の安定版リリースタグを使う (`git fetch --tags && git checkout vX.Y.Z && git submodule update --init --recursive && ./install.sh esp32p4`)。
ベンダーデモは **ESP-IDF v5.5.4** (`1-Demo/IDF_5.5.4`) と **v6.0.2** (`1-Demo/P4V3X_demo/IDF_6.0.2`) 向けで、
IDF 6.x でのコンポーネント分割に追従したものは後者を参照する。

```bash
# 環境読み込み (毎シェル)
. ~/esp/esp-idf/export.sh

# プロジェクト初期化〜書き込み
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p /dev/cu.usbserial-110 flash monitor   # Ctrl-] で monitor 終了

# 部分的なコマンド
idf.py -p /dev/cu.usbserial-110 flash
idf.py -p /dev/cu.usbserial-110 monitor
idf.py fullclean
idf.py save-defconfig       # sdkconfig.defaults を更新
```

書き込み後にデバイスがフラッシュモードに入らない場合は電源を入れ直す (ベンダー Getting Started の注意)。

`firmware/p4/sdkconfig.defaults` の要点:

- **手元の基板の P4 はチップリビジョン v1.3** (`esptool chip-id` で確認済み)。IDF v6.1 の既定は Rev 3.01 以上なので
  `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` と `CONFIG_ESP32P4_REV_MIN_100=y` を入れている。これが無いとブートしない。
  参照するベンダーデモは Rev1 向けの `1-Demo/IDF_5.5.4/` 側 (`P4V3X_demo` は Rev3 基板用)。
- P4 のブートローダは 0x2000〜0x8000 の 24 KB しか無く、既定のログレベルだと溢れる。`CONFIG_BOOTLOADER_LOG_LEVEL_WARN=y` で収めている。
- 16 MB QIO flash、PSRAM 200 MHz (実測 32 MB)、FreeRTOS 1000 Hz、`CONFIG_IDF_EXPERIMENTAL_FEATURES=y`。
- パーティションは `partitions.csv` (factory 8 MB + FAT `storage` 約 8 MB)。

IDF Component Manager (`main/idf_component.yml`) で `espressif/esp_lcd_jd9365` を取得する。
`esp_lcd_touch_gsl3680` はベンダー `common_components/` にしか無いので、必要なら `components/` へコピーして `EXTRA_COMPONENT_DIRS` に加える。

### PlatformIO

`pio` は `/usr/local/bin/pio` にインストール済みだが、**公式 `espressif32` プラットフォームは ESP32-P4 非対応**。
使うなら pioarduino フォーク (`platform = https://github.com/pioarduino/platform-espressif32/...`) を指定する。
主経路は ESP-IDF の `idf.py` とする。

### LovyanGFX と MIPI-DSI

LovyanGFX の ESP32-P4 MIPI-DSI 対応は **develop ブランチの実験的実装** で、リリース版には無い (2025 時点)。
参照実装は M5GFX の M5Stack Tab5 (ESP32-P4 + ILI9881C, DSI) 用パネル定義。
JD9365 用の Panel クラスを自前で書く場合は、esp-idf の `esp_lcd_mipi_dsi` でバス/DPI パネルを作り、
そのフレームバッファに対して LovyanGFX の描画を行う形 (ベンダーデモと同じ構成: DSI bus → DBI IO → DPI panel) を基本にする。
LovyanGFX が間に合わない部分は `esp_lcd_panel_draw_bitmap` 直叩きで先に動かしてよい。

## リポジトリ構成

```
firmware/
  p4/              ESP32-P4 メインファームウェア (ESP-IDF プロジェクト)。idf.py はここで実行する
    main/          app_main と配線のみ。ロジックは components/ に置く
    components/
      board/       JC8012P4A1 のピン定義 (board_pins.h)。今後 LCD/SD/codec の初期化もここ
      (予定) display/      LovyanGFX と esp_lcd DSI パネルの接続、回転、フレームバッファ
      (予定) video_player/ MP4 demux → デコード → PPA → 表示
      (予定) status_bar/   下 80px の UI
  c6/              ESP32-C6 用。現状は空 (ESP-Hosted スレーブは標準ファームを使う)
docs/              ベンダー資料 (参照専用)
```

- 2 チップ構成だが、**C6 に独自ファームは当面書かない**。P4 側で `espressif/esp_hosted` + `esp_wifi_remote` を使い、
  C6 は Espressif 標準の ESP-Hosted スレーブファーム (工場出荷の `JC-C6-slave_*.bin`) をそのまま使う。
  C6 に Wi-Fi/BT 中継以外の役割を持たせる必要が出た時だけ `firmware/c6/` にプロジェクトを作る。
- 機能ごとに `firmware/p4/components/<feature>/` を切る。`main/` に実装を書かない。
- `sdkconfig` は生成物で commit しない。設定変更は `sdkconfig.defaults` に `idf.py save-defconfig` で反映する。
- `docs/JC8012P4A1C_I_W_Y/` — ベンダー配布物。**ビルドや改変はここで行わない** (参照専用、必要なものはコピーして使う)。
  - `1-Demo/IDF_5.5.4/{Old,New}_Panel/` — ESP-IDF デモ (`lvgl_demo_v8/v9`, `esp_draw_bit`, `esp_brookesia_phone`, `usb_extend_screen`, `video_lcd_display`, `xiaozhi-esp32-main`) と `common_components/` (jd9365 / gsl3680 / BSP)
  - `1-Demo/P4V3X_demo/IDF_6.0.2/` — 同内容の v6.0.2 版 (P4 Rev3 向け)
  - `1-Demo/arduino-examples/` — Arduino 版 (`pins_config.h` にピン定義)
  - `1-Demo/MJPEG/video1.mjpeg` + `video1.wav` — ベンダーの動画デモ素材 (MJPEG + WAV 分離形式)
  - `5-Schematic/*.png`, `JC8012P4A1.pdf` — 回路図 (LCD, C6, CONN, CODEC, USB, PWR)
  - `4-Driver_IC_Data_Sheet/` — JD9365 初期化コード (txt), OV02C10, P4 データシート
  - `8-Burn operation/Burn files/*.bin` — 工場出荷ファーム (復旧用。`JC-C6-slave_*.bin` は C6 側)
- 一部デモディレクトリに `.git/` が入ったまま同梱されている (例: `video_lcd_display/.git`)。そのまま commit すると
  ネストした Git リポジトリ扱いになるので、`docs/` を commit する際は除去するか `.gitignore` で扱いを決める。

## 動画再生の設計メモ

- **動画フォーマットは MJPEG に決定** (2026-11-06)。ESP32-P4 の H.264 はハードウェアエンコーダのみで、デコードは
  `esp_h264` のソフトウェア実装 (Baseline 限定、720p で 7〜10 fps) しかなく 720p30 は不可能。
  P4 のハードウェア JPEG デコーダ (`esp_driver_jpeg`) を使う MJPEG はベンダーデモで 720p30 実証済み。
- 変換パラメータ: `ffmpeg -i in.mp4 -an -vf fps=30 -c:v mjpeg -q:v 12 -pix_fmt yuvj420p out.mjpeg`。
  `elis.mp4` (H.264 Main 1280x720 29.97fps 2.1 Mbps, AAC-LC 44.1 kHz stereo, 30 秒 7.9 MB) は約 22.8 MB (2.9 倍) になる。
  q=12 で PSNR 32.7 dB、見た目は元と同等。ユーザーは 30fps 優先で約 3 倍のサイズを受け入れている。
- 音声は元の AAC をどう扱うか未決 (`esp_audio_codec` の AAC デコーダ、または事前に WAV/PCM へ変換)。
- 再生パイプライン: SD (SDMMC 4-bit, `esp_vfs_fat_sdmmc_mount`) → MJPEG フレーム切り出し (FFD8..FFD9) → HW JPEG デコード (RGB565)
  → PPA SRM で 270° 回転して 1280x720 を配置 → DSI フレームバッファ (ダブルバッファ)。
- ベンダーの `video_lcd_display` は **カメラ→LCD** のデモであり、ファイル再生ではない。流用できるのは
  LCD 初期化 (`app_lcd.c`)、PPA による回転/スケール、ダブルバッファの `esp_lcd_dpi_panel_get_frame_buffer` の使い方。
- 1280x720 を横向き 1280x800 の上端に配置し、下 80px (y=720..799) をステータスバーとして描画する (LovyanGFX の担当領域)。
