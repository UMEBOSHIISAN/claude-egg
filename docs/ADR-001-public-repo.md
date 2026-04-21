# ADR-001: repo を public で公開する

**Date:** 2026-04-21  
**Status:** ACCEPTED

## Context

claude-egg は Phase A + Phase B が動作する状態で、既に `UMEBOSHIISAN/claude-egg` として GitHub public repo に push されている。
配布前提の設計（swap surface・buddies pack 方式）であることを踏まえ、visibility の方針を明示する。

## Decision

**Public で継続する。**

Push 時点で既に public を選択しており、その判断を正式に承認する。

## 理由

- claude-egg の核心設計は「swap surface を広く開ける」こと。公開してこそ pack author が増える
- anthropics/claude-desktop-buddy・m5stack/M5Cardputer の両 upstream が公開 OSS であり、同じ場に立つことが適切
- ライセンス境界は scaffold 時点で確定済み（core=MIT / buddies/default=MIT / buddies/shisoko=CC-BY-NC-4.0）
- 本番パイプライン（xops / pm2 / approval_journal / metrics.db）と完全に独立しており、公開による実害なし

## 影響する設計方針

### README
- 公開読者を想定した書き方で継続する（現状の launch-ready README はこの前提で書かれている）
- secrets・config のパスをハードコードしない

### secrets / config
- `~/.secrets/` や環境変数への参照はコード内に書かず、README の「setup」セクションで案内する
- tools/ 配下のスクリプトに personal path をハードコードしない（`$HOME` / argparse で受け取る）

### telemetry / usage データ
- `claude_usage_digest.py` が読むのはローカルの Claude Code ログのみ
- 外部への送信なし・収集なし。この方針は変えない
- heartbeat は BLE ローカル通信のみ

### firmware / asset 同梱方針
- firmware（src/）は repo に含める
- sprite/GIF アセットは buddies/<name>/ 配下に置く。CC-BY-NC-4.0 の shisoko pack は public repo に含めない
- Phase C（LittleFS GIF）着手時も同じ境界を維持する

## 非対象

- cardputer-buddy の fork 化（別 ADR）
- Claude-EGG を Mother Egg 観測装置として接続する設計（別タスク・観測フェーズ完了後）
