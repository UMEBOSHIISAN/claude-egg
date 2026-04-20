# Claude-EGG

> **Claude-EGG is a "character swap architecture" for Claude-connected devices.
> Core is minimal. Expression is replaceable.**

A physical pet that hatches out of your Claude Code sessions and lives on an M5Stack Cardputer.

It starts as an egg. The more you code with Claude, the more it grows — or gets exhausted, or throws a tantrum when you push it past midnight. Swap the sprite pack and the dialogue pack, and it becomes *your* creature.

> **Status:** Design scaffold. Not a runnable firmware yet. The full spec, including why the upstreams look the way they do and how packs degrade gracefully, lives in [`docs/DESIGN.md`](docs/DESIGN.md). **Read that before forking.**

## Why this exists

Two upstream projects already do most of the hard work:

- **[anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy)** — Anthropic's reference BLE buddy that visualizes live Claude session state (busy/idle/permission). MIT-licensed. Designed to be forked with custom characters.
- **[m5stack/M5Cardputer](https://github.com/m5stack/M5Cardputer)** — the device this runs on. QWERTY + 240×135 TFT + ESP32-S3. LittleFS-first asset pipeline, BLE via NimBLE.

Both upstreams share the same design philosophy: **keep the core small, make the swap surface wide.** Anthropic's buddy ships with a single sample character so you fork and reskin. M5Stack's firmware pattern is PlatformIO + LittleFS so assets are swappable without a recompile.

Claude-EGG takes that swappable surface and pushes it further: the pet doesn't just *display* session state, it **accumulates** it. Usage time is the food. Long sessions grow it. Silence starves it. 3 a.m. coding sessions make it sick.

## Concept in one paragraph

A background daemon on your Mac tails `~/.claude/projects/*/` session logs, rolls up today's Claude Code active minutes, and pushes a JSON heartbeat over BLE NUS to a Cardputer sitting on your desk. The Cardputer renders an animated sprite whose **life stage** (egg → child → teen → adult → elder) is a function of cumulative lifetime usage, and whose **mood** (happy / tired / grumpy / sick) is a function of today's pattern — burst, marathon, graveyard-shift, silent. Swap `buddies/<name>/` on the host and it's a different creature tomorrow.

## Swap surface

Everything that makes the pet "yours" is in two places:

```
buddies/<name>/
├── manifest.yaml      # name, author, license, state → frame mapping
├── sprites/           # 240×135 GIFs, one per (stage, mood) pair
└── lines/             # short dialogue strings, one YAML per mood
```

No firmware rebuild needed for sprite or line swaps — everything flashes to LittleFS.

The default pack is a generic pixel blob so forks are frictionless. `buddies/shisoko/` is the UMEBOSHI reference pack and doubles as a worked example of the manifest spec.

## Fork guide (short version)

1. Fork this repo.
2. `cp -r buddies/default buddies/<your-name>`
3. Replace the sprites, edit `lines/*.yaml`, bump `manifest.yaml`.
4. Set `DEFAULT_BUDDY=<your-name>` in `platformio.ini` build flags, flash.
5. Open a PR to add your pack to the community list in [`buddies/README.md`](buddies/README.md) if you want it discoverable.

## Not in scope

- On-device LLM inference (the Module LLM board is the right place for that, not the Cardputer itself).
- Sending your session transcripts to a server. The daemon only emits aggregate counters — minutes, session count, hour buckets. No prompts, no responses, no file paths.
- Replacing `claude-desktop-buddy`. That project is a general buddy; Claude-EGG is narrowly a creature-that-grows. Different shape.

## License Notice

- **Core code:** MIT License — see [`LICENSE`](LICENSE).
- **Assets under `/buddies/*`:** Each pack has its own license. Check the `license:` field in that pack's `manifest.yaml` before reusing.

> ⚠️ **Do NOT assume all contents are MIT.** The repo code is MIT; the included `buddies/default/` pack is MIT; but other packs (including `buddies/shisoko/`) are under their own terms and may be non-commercial or all-rights-reserved. Redistributing a pack under MIT when its manifest says otherwise is the #1 way this kind of project breaks.

See [`docs/DESIGN.md`](docs/DESIGN.md) for the full specification.
