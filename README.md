# Claude-EGG

> **A physical pet that lives on an M5Cardputer and grows from your Claude Code sessions.
> Swap the sprite pack, swap the dialogue pack, swap the growth rules — the pet becomes *yours*.**

> **Status:** Design scaffold. No runnable firmware yet. Full spec lives in [`docs/DESIGN.md`](docs/DESIGN.md). **Read it before forking.**

## What the default pack does

Out of the box, Claude-EGG behaves like this:

- 🍚 **You code with Claude → it eats.** Cumulative Claude Code active minutes drive **life stage** (egg → child → teen → adult → elder). Stage transitions are **one-way**.
- 🧈 **Today's hours shape its body.** Long sessions → puffy / chonk. Silent days → gaunt / dried out. A healthy rhythm → default shape.
- 🥵 **3 a.m. grinds make it sick.** Activity after 01:00 for two nights in a row flips the pet into a **belly-up "please sleep" mood** with X eyes. It's not a safety alarm (those have their own rules). It's a pet having a normal reaction to neglect.
- 📋 **Morning postmortem on the Cardputer.** Press a key at wake-up and it shows yesterday's diagnosis: hours, late-night minutes, streak status.
- 😶 **Silence has consequences too.** 24 h idle → lonely. 7 days idle → zen (it has accepted its abandonment).

None of this is hardcoded — every threshold, every sprite, every line is in a **pack** under `buddies/`. Fork this repo, copy `buddies/default/`, and the pet becomes whatever you want it to be.

## Why this exists

Two upstreams already do most of the hard work:

- **[anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy)** — Anthropic's reference BLE buddy that visualizes live Claude session state (busy / idle / permission). MIT-licensed. Designed to be forked with custom characters.
- **[m5stack/M5Cardputer](https://github.com/m5stack/M5Cardputer)** — the device this runs on. QWERTY + 240×135 TFT + ESP32-S3. LittleFS-first asset pipeline, BLE via NimBLE.

Both upstreams share the same philosophy: **keep the core small, make the swap surface wide.** Claude-EGG extends that — the pet doesn't just *display* session state, it **accumulates** it. Usage time is food. Silence starves. 3 a.m. coding makes it sick.

## Concept in one paragraph

A background daemon on your Mac tails `~/.claude/projects/*/` session logs, rolls up today's Claude Code active minutes, and pushes a JSON heartbeat over BLE NUS to a Cardputer on your desk. The firmware renders an animated sprite whose **life stage** is a function of cumulative lifetime usage, and whose **mood** is a function of today's pattern — burst, marathon, graveyard-shift, silent. Swap `buddies/<name>/` on the host and it's a different creature tomorrow.

## Swap surface

Everything that makes the pet "yours" lives under `buddies/`:

```
buddies/<name>/
├── manifest.yaml      # name, author, license, stage thresholds, state → frame mapping
├── sprites/           # 240×135 GIFs, one per (stage, mood) pair
└── lines/             # short dialogue strings, one YAML per mood
```

Three independent swap axes:

| Axis | File | What changes |
|------|------|--------------|
| **Look** | `sprites/*.gif` | Pixel art, cartoon, ASCII, photo — anything 240×135 |
| **Voice** | `lines/*.yaml` | Tone, language, vibe (stern mum? hype bro? cat?) |
| **Growth curve** | `manifest.yaml:stages` | Faster creature, slower creature, different stage names visible in sprites |

No firmware rebuild needed for any of it — everything flashes to LittleFS.

## Make it yours in 3 minutes

```bash
# 1. Fork this repo, clone your fork.
# 2. Copy the default pack and rename.
cp -r buddies/default buddies/<your-name>

# 3. Replace sprites/*.gif, edit lines/*.yaml, bump manifest.yaml.
# 4. Flash:
pio run -t uploadfs -e m5cardputer

# 5. On-device: press `b` to cycle packs, or set DEFAULT_BUDDY in platformio.ini.
```

See [`buddies/README.md`](buddies/README.md) for the pack author guide.

## Built-in packs

- [`buddies/default/`](buddies/default/) — MIT-licensed generic placeholder. Fork template.
- [`buddies/tartan/`](buddies/tartan/) — **タータン**, UMEBOSHI's reference pack. A frog that puffs up when well-fed and turns belly-up at 3 a.m. CC-BY-NC-4.0 — worked example of a branded character.

## Not in scope

- On-device LLM inference (the Module LLM board is the right place, not the Cardputer).
- Sending session transcripts anywhere. The daemon only emits aggregate counters — minutes, session count, hour buckets. No prompts, no responses, no file paths.
- Replacing `claude-desktop-buddy`. That project is a general buddy. Claude-EGG is narrowly *a creature that grows*. Different shape.
- Pet death. Stages are one-way but the pet never dies. Neglect makes it sad, not gone.

## License Notice

- **Core code:** MIT License — see [`LICENSE`](LICENSE).
- **Assets under `buddies/*`:** Each pack has its own license. Check the `license:` field in that pack's `manifest.yaml` before reusing.

> ⚠️ **Do NOT assume all contents are MIT.** The repo code is MIT; the included `buddies/default/` pack is MIT; but other packs (including `buddies/tartan/`) are under their own terms and may be non-commercial or all-rights-reserved. Redistributing a pack under MIT when its manifest says otherwise is the #1 way this kind of project breaks.

See [`docs/DESIGN.md`](docs/DESIGN.md) for the full specification.
