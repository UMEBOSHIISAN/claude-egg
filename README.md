# Claude-EGG

**A physical pet that lives on an M5Stack Cardputer and grows from your Claude Code sessions.** Fork the repo, swap the sprite pack, the dialogue pack, or the growth rules — the pet becomes *yours*.

---

> **Status: Phase A + Phase B running end-to-end today.**
> The Cardputer flashes in two minutes (shapes-based renderer, no assets yet). A macOS background daemon tails your Claude Code session logs, rolls up today's active minutes + late-night streak, and pushes a JSON heartbeat to the device over BLE NUS every 10 seconds. The pet's stage and mood update in real time from your actual usage. Phase C swaps the shape renderer for LittleFS-backed GIFs. See [the roadmap](#roadmap) below.

---

## Why this exists

Two upstream projects had already done most of the hard work, and between them they told you exactly what was missing:

- **[anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy)** is Anthropic's reference BLE buddy. It renders live Claude session state — busy / idle / permission-needed — and answers permission prompts with a physical button. MIT-licensed. The whole project ships with a single generic character and a note in the README that reads, roughly: *fork this and put your own creature on it.* The reference hardware is M5StickCPlus.
- **[m5stack/M5Cardputer](https://github.com/m5stack/M5Cardputer)** is a credit-card-sized ESP32-S3 with a 56-key QWERTY keyboard and a 240×135 TFT. The M5Stack firmware pattern is PlatformIO plus LittleFS so that character assets flash separately from firmware. You write the code once; pack authors ship GIFs after that.

Both projects share one design principle: **keep the core small, make the swap surface wide.** Anthropic's buddy is a faceplate for Claude. M5Stack's firmware pattern is a faceplate for… whatever you want to put on the LCD.

Claude-EGG takes that swappable surface and pushes it further. The pet doesn't just *display* session state — it **accumulates** it. Your Claude usage is the food. Silence starves it. 3 a.m. coding sessions make it sick. It grows through five life stages that are one-way and permanent for a given pet identity, so the stakes feel real without the project ever becoming a safety tool. Swap `buddies/<name>/` on the host and tomorrow it's a completely different creature eating the same minutes.

## What the default pack does

Out of the box, Claude-EGG behaves like this:

### Food

Your cumulative Claude Code active minutes drive **life stage**:

| Stage       | Threshold       | Approximate calendar |
|-------------|-----------------|----------------------|
| egg         | 0 – 30 min      | day 0                |
| tadpole     | 30 min – 10 h   | first week           |
| frog        | 10 h – 50 h     | first month          |
| toad        | 50 h – 300 h    | first quarter        |
| pond-sage   | 300 h +         | lifer                |

Stage transitions are **permanent**. A pond-sage never goes back to being an egg. If you want a fresh pet, you start a new buddy profile — the repo supports multiple. This one un-fun rule is what makes the other rules feel like they matter.

### Shape

Today's usage shape the pet's **body**:

- Under-eating for days → the frog gets **gaunt**, dry, washed out.
- Over-eating today → it **puffs** into a chonk variant.
- Steady, healthy rhythm → default shape.

Body is independent of stage. A gaunt pond-sage and a chonky tadpole are both valid states, and both have their own sprite frames.

### Mood

Today's *pattern* drives **mood**, which recomputes every heartbeat:

| Mood    | What triggers it                                                        |
|---------|-------------------------------------------------------------------------|
| happy   | 1–4 h today, mostly in daylight hours                                    |
| excited | Active session *right now*, in short bursts                              |
| tired   | 4–8 h today                                                              |
| grumpy  | 8 h + today, or 3+ days in a row with 6 h+                               |
| sick    | Activity after 01:00 local time, 2+ nights in a row                      |
| lonely  | 0 min for 24 h                                                           |
| zen     | 0 min for 7+ days (the pet has accepted its abandonment)                 |

The interesting moods are **grumpy** and **sick**. Grumpy answers "you worked too much *in total*." Sick answers "you worked at the wrong *time*." Those two axes — amount and time-of-day — are separate in real life, and they're separate here too. A frog belly-up in a puddle with X eyes is the pet's normal reaction to you pushing a session past 3 a.m. for the second night in a row; it is not a safety alarm. Safety alarms have their own rules in this repo owner's personal system and they're kept far away from the toy.

### Silence

Zero activity has consequences too. 24 hours of silence → the pet turns lonely, staring at an empty pond. Seven days → it goes zen and just sits there serenely. The pet **never dies**, ever. That's a deliberate choice — a tool that yells at you for taking a week off from Claude would be its own kind of safety alarm, and safety alarms belong elsewhere.

### Postmortem

Press a key in the morning and the Cardputer shows you **yesterday's diagnosis**: active minutes, late-night minutes, current streak, silent-hour total. This is the one place where the pet breaks character and talks to you straight.

None of the rules above are hardcoded in the firmware. All of it is declarative and lives in the **pack**.

## Hardware

- **M5Stack Cardputer** (ESP32-S3, 8 MB PSRAM, 240×135 TFT, 56-key QWERTY). Cardputer-only by design. The StickCPlus example from the upstream `claude-desktop-buddy` project is **not** supported by this firmware — the keyboard-based pack switcher and the sprite geometry both assume the Cardputer form factor. If you want a StickCPlus buddy, the upstream reference is the place to start.
- **Host: macOS.** The Phase A demo runs without a host, but Phase B (real usage data) expects a Mac daemon reading `~/.claude/projects/*/`. Linux and Windows ports are not planned for v1 but the daemon is a plain Python script with no macOS-specific dependencies — porting is expected to be a one-afternoon job for someone who wants it.

## Quick start — two minutes

```bash
# Prereq: PlatformIO Core. One-time:
brew install platformio       # macOS

# Get the repo and flash.
git clone https://github.com/UMEBOSHIISAN/claude-egg.git
cd claude-egg
pio run -e m5cardputer -t upload
```

On reset you'll see the `tartan` frog centered on the screen in the default `teen / happy` state. The header reads `Claude-EGG :: default`; the footer shows the current stage, mood, and minute counters.

### Phase A keyboard controls

Phase A has no BLE yet, so the Cardputer's QWERTY keyboard drives the state directly:

| Key              | What it does                                                                    |
|------------------|---------------------------------------------------------------------------------|
| `1` `2` `3` `4` `5` | stage = egg / tadpole / frog / toad / pond-sage                               |
| `q` `w` `e` `r` `t` `y` | mood = happy / excited / tired / grumpy / sick / lonely                  |
| `a`              | toggle the "active now" indicator — an amber pulse dot at the top right         |
| `+` / `-`        | bump today+lifetime minutes by ±30 (watch the footer update)                    |

**Highlights to try first:** `3` then `t` for the 3 a.m. belly-up sick frog with X eyes. `5` for the pond-sage. `1` for the egg (watch for the crack on the top when `late_night_streak` is nonzero).

The keyboard keeps working as a debug override even when the daemon is connected — press any of the keys above and the header shows `MANUAL` in amber to say "heartbeat-driven state is frozen." Press `0` to release the override and let heartbeats drive the state again.

## Run the daemon — five minutes

The daemon is what makes the pet actually *live* on your Claude usage. Without it, Phase A's keyboard demo is the whole experience.

```bash
# One-time: set up a venv and install bleak.
cd tools
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
cd ..

# Start it (keeps running until you Ctrl-C):
tools/.venv/bin/python tools/claude_usage_digest.py
```

On first run macOS will prompt you to allow Bluetooth access for your terminal (or for Python itself, depending on how you launched it). Grant it. The daemon will then scan for the Cardputer advertising as `Claude-EGG`, connect, and start pushing one heartbeat every 10 seconds:

```
21:46:30  INFO    scanning for 'Claude-EGG'...
21:46:30  INFO    connected to 71FFB59E-...
21:46:34  INFO    heartbeat: life=9908 today=501 active=True late_streak=2 silent_h=0
```

On the Cardputer, the BLE indicator (top-right dot) flips from magenta to **green** when the daemon connects. The pet's stage and mood shift to match your real usage. State is persisted at `~/Library/Application Support/claude-egg/state.json` between runs.

To keep the daemon running across reboots, install it as a launchd agent — see [`examples/com.umeboshi.claude-egg.plist`](examples/com.umeboshi.claude-egg.plist) for a ready-to-edit template.

## The swap surface

Everything that makes the pet "yours" lives under `buddies/`:

```
buddies/<name>/
├── manifest.yaml   # name, author, license, stage thresholds, frame mapping
├── sprites/        # 240×135 GIFs, one per (stage, mood) pair    [Phase C]
└── lines/          # short dialogue strings, one YAML per mood   [Phase C]
```

Three independent swap axes, each of which you can touch without asking anyone's permission:

### 1. Look — `sprites/*.gif`

Pixel art, cartoon, ASCII creature rendered into a GIF, photograph, abstract geometry — anything that fits in 240×135. Filenames are keyed `<stage>_<mood>.gif`. Missing pairs fall back to `<stage>_default`, then to a global default, so a minimal pack can ship five sprites (one per stage) and still render every mood without crashes.

### 2. Voice — `lines/*.yaml`

Short dialogue strings, grouped by mood. A pack ships a stern mum, a hype bro, a cat, a Shakespearean villain — whoever the pack author wants. The firmware pulls a weighted random line keyed on the current day, so the same mood doesn't spam the same line.

### 3. Growth curve — `manifest.yaml:stages`

The *thresholds* (minutes to reach each life stage) live in the manifest. A pack can be faster-growing (egg → elder in a week), slower-growing (a six-month lifer arc), or entirely different — `stages` are key-value pairs, so a pack author can rename them in the sprite filenames and comments, telling a different story with the same state machine. The tartan pack uses `tadpole / frog / toad / pond-sage`; a dragon pack might use `hatchling / drake / wyrm / ancient`.

No firmware rebuild is needed for sprite or line swaps — those flash to LittleFS via `pio run -t uploadfs` separately. A growth-curve swap is a manifest edit, same thing.

## Make it yours in three minutes

```bash
# 1. Fork this repo, clone your fork.
# 2. Copy the default pack and give it a name.
cp -r buddies/default buddies/<your-name>

# 3. Replace sprites/*.gif, edit lines/*.yaml, bump manifest.yaml:
#      name, display_name, author, license, version, stage thresholds.

# 4. Point the firmware at your pack and flash both the firmware and the FS.
pio run -e m5cardputer -D DEFAULT_BUDDY=\"<your-name>\" -t upload
pio run -e m5cardputer -t uploadfs
```

Everything else — the pack authoring checklist, license choices, and submission criteria for the community-packs list — lives in [`buddies/README.md`](buddies/README.md).

## Built-in packs

- [`buddies/default/`](buddies/default/) — MIT-licensed generic placeholder. Fork template. The intended design is that a new forker's first commit is literally `cp -r buddies/default buddies/<their-name>`.
- [`buddies/tartan/`](buddies/tartan/) — **タータン (Tartan)**, UMEBOSHI's reference pack. A frog that puffs up when well-fed, dries out when starved, and turns belly-up with X eyes after two nights of 3 a.m. grinding. **CC-BY-NC-4.0**, non-commercial redistribution only. Shipped as the worked example of what a branded character pack looks like — both the art direction and the license are representative of how a brand-owning forker should ship.

## Architecture

```
┌─────────────────────────────────────┐            ┌───────────────────────────────┐
│  Host: macOS (Phase B)              │            │  M5Stack Cardputer            │
│                                     │            │                               │
│  ┌───────────────────────────────┐  │            │  ┌─────────────────────────┐  │
│  │ claude_usage_digest.py        │  │  BLE NUS   │  │ NimBLE NUS server       │  │
│  │  • tails ~/.claude/...jsonl   │  │  JSON      │  │                         │  │
│  │  • rolls up today's minutes   │──┼──1 / 10s──▶│  │ state machine           │  │
│  │  • detects late-night streaks │  │            │  │   (stage × mood × body) │  │
│  │  • emits heartbeat JSON       │  │            │  │                         │  │
│  └───────────────────────────────┘  │            │  │ pack system             │  │
│                                     │            │  │   (LittleFS, manifest,  │  │
│  ┌───────────────────────────────┐  │            │  │    sprites, lines)      │  │
│  │ launchd agent                 │  │            │  │                         │  │
│  │   com.umeboshi.claude-egg     │  │            │  │ renderer                │  │
│  └───────────────────────────────┘  │            │  │   (shapes or GIF)       │  │
│                                     │            │  └─────────────────────────┘  │
└─────────────────────────────────────┘            └───────────────────────────────┘
```

Two properties to call out explicitly:

- **The firmware never talks to Anthropic, or to any cloud, ever.** The only IO the pet does is BLE NUS to your Mac and TFT output to its own display. There is no WiFi stack in this firmware.
- **The daemon is read-only on your session logs.** It tails `~/.claude/projects/**/*.jsonl`, derives aggregate counters, and writes its own state to `~/Library/Application Support/claude-egg/state.json`. Session transcripts never leave that directory. Counters are: minutes, session count, hour buckets. No prompts, no responses, no file paths.

## BLE NUS protocol (Phase B)

The host daemon emits one heartbeat every 10 seconds over the Nordic UART Service. Payload is a single line of JSON:

```json
{
  "type":              "egg.heartbeat",
  "v":                 1,
  "lifetime_min":      12847,
  "today_min":         412,
  "active_now":        true,
  "late_night_streak": 2,
  "silent_hours":      0,
  "ts":                "2026-04-20T14:33:02Z"
}
```

Fields:

| Field                | Meaning                                                                   |
|----------------------|---------------------------------------------------------------------------|
| `type`               | Always `egg.heartbeat`. Reserved so other message types can coexist.       |
| `v`                  | Protocol version. Firmware rejects packets it doesn't recognize.           |
| `lifetime_min`       | Cumulative active minutes across the pet's entire lifetime. Monotonic.     |
| `today_min`          | Active minutes so far today (local midnight to now).                       |
| `active_now`         | True if there was activity in the last 60 s. Drives the amber pulse dot.   |
| `late_night_streak`  | How many consecutive nights had activity after 01:00 local time.           |
| `silent_hours`       | Hours since the last activity. Drives lonely / zen transitions.            |
| `ts`                 | UTC ISO-8601 timestamp. Firmware only uses this for ordering, not display. |

The firmware computes **stage** from `lifetime_min`, and **mood** from the remaining fields plus its own clock. Stage thresholds come from the manifest on the *device* side, which means swapping a pack changes growth curves **without restarting the daemon**.

The daemon is stateless with respect to the pet. All pet identity — thresholds, frame mapping, lines — is on the device. This is a deliberate inversion of the usual host-is-smart / device-is-dumb pattern, and it exists so that pack authors can ship their own growth curves without touching daemon code.

## Roadmap

### Phase A — local demo firmware ✅ shipped
- Cardputer boots, renders the `tartan` frog with shapes.
- QWERTY keys drive stage / mood so development works without a host daemon.
- No BLE, no LittleFS assets yet — pure render loop.

### Phase B — host daemon + BLE heartbeat ✅ shipped
- `tools/claude_usage_digest.py` tails `~/.claude/projects/**/*.jsonl`, rolls up today's active minutes, detects late-night streaks, and pushes the heartbeat JSON above every 10 s over BLE NUS.
- Firmware computes stage from `lifetime_min` and mood from the rest (priority: SICK > ZEN > LONELY > GRUMPY > TIRED > EXCITED > HAPPY). Keyboard keeps working as a debug override (press `0` to release).
- State persisted to `~/Library/Application Support/claude-egg/state.json` between runs.
- launchd agent template ships at [`examples/com.umeboshi.claude-egg.plist`](examples/com.umeboshi.claude-egg.plist).

### Phase C — LittleFS asset swaps
- AnimatedGIF + LittleFS renderer replaces the current shape renderer.
- Pack authors ship real GIFs. The default pack gets a placeholder pixel blob; the `tartan` pack gets proper frog art.
- `pio run -t uploadfs` ships the pack without a firmware rebuild.

### Later — not blocking a v1 public launch
- **Pack layering.** Today a pack owns sprites + voice + rules as a single bundle. A future format lets you mix them: *tartan sprites + hype voice + idle rules* as three independent choices.
- **Pack discovery.** A community pack index fetched over HTTPS so the device can list available packs without requiring a reflash.
- **Morning postmortem view.** Press a key at wake-up → yesterday's full diagnosis screen.
- **CLI companion.** A `claude-egg today` / `claude-egg stats` CLI that prints the same numbers the pet sees, for when the Cardputer is charging or traveling.

## Not in scope

- **On-device LLM inference.** The M5Stack Module LLM board is the right place for that, not the Cardputer itself. The Cardputer has no brain; the Mac does. The pet has a face.
- **Session transcript transmission.** Aggregate counters only. Ever.
- **Replacing `claude-desktop-buddy`.** That project is a general buddy for Claude session state. Claude-EGG is narrowly *a creature that grows*. Different shape, different problem.
- **Pet death.** Stages are one-way, but the pet never dies. Neglect makes it sad. That's a ceiling on the emotional stakes on purpose.
- **Streaks as rewards.** No Duolingo-style lose-your-streak mechanics. Turning Claude usage into a habit-trap is a failure mode, not a feature.

## Project layout

```
claude-egg/
├── README.md                      ← this file
├── LICENSE                        ← MIT, code only
├── platformio.ini                 ← firmware build config
├── src/
│   └── main.cpp                   ← Phase A firmware
├── tools/
│   ├── README.md                  ← host daemon guide
│   ├── claude_usage_digest.py     ← Phase B daemon
│   └── requirements.txt
├── examples/
│   └── com.umeboshi.claude-egg.plist  ← launchd agent template
├── data/                          ← LittleFS image staging (Phase C)
│   └── chars/
├── docs/
│   └── DESIGN.md                  ← full specification
└── buddies/
    ├── README.md                  ← pack author guide
    ├── default/                   ← MIT placeholder pack
    │   └── manifest.yaml
    └── tartan/                    ← UMEBOSHI reference pack (frog, CC-BY-NC-4.0)
        └── manifest.yaml
```

## Troubleshooting

**The Cardputer screen stays black after flashing.** Press the side reset button once. First-boot resets sometimes hang between flash and app-start; a manual reset is enough. If the screen still stays dark, check that `platformio.ini` points at `m5cardputer` and re-run `pio run -e m5cardputer -t upload`.

**The daemon says `Bluetooth device is turned off`.** macOS Bluetooth is off or the terminal doesn't have Bluetooth permission. Open System Settings → Privacy & Security → Bluetooth, make sure Terminal (or whichever shell you launched the daemon from) is allowed. Flip BT off and on if the list looks right but the daemon still fails.

**The daemon says `device 'Claude-EGG' not found`.** The Cardputer isn't advertising. Reset it (side button), wait ~5 seconds for BLE to come up, and the daemon will retry automatically on the next loop. If it still can't find it, check that the firmware actually booted — the header should read `Claude-EGG :: default`. If you see a blank screen, re-flash.

**Heartbeats are flowing but the pet isn't changing.** The keyboard override is probably stuck on. Look at the header: if you see a `MANUAL` badge in amber, the keyboard took control and froze the heartbeat-driven state. Press `0` on the Cardputer to release.

**`pip install bleak` fails with a pyobjc warning.** That warning is harmless on macOS and doesn't prevent install. If it genuinely errors out, upgrade pip first: `tools/.venv/bin/pip install --upgrade pip`, then retry.

**The pet's stage / mood doesn't match what I expect.** Stage comes from `lifetime_min` (cumulative minutes). Mood comes from today's minutes, late-night streak, and silence. Run the daemon once with `INFO` logging (the default) and you'll see the exact numbers it's sending — compare those against the tables in [What the default pack does](#what-the-default-pack-does).

**The daemon dies when the Cardputer sleeps / is unplugged.** Expected. The daemon reconnects automatically when the device comes back. If you want it supervised across reboots, use the launchd agent template in `examples/`.

## Contributing

The contribution surface sorts into three categories. They have different bars.

**Pack contributions (easy).** Anyone is welcome to open a PR adding their pack to the community list in [`buddies/README.md`](buddies/README.md). The only hard requirements are: (a) the manifest validates, (b) the pack declares its license, and (c) at least three stages have custom sprites so it's not just a reskin of `default`.

**Firmware changes (medium).** Keep it Cardputer-only. Don't regress the shapes renderer in Phase A — the low-effort bring-up path matters. Prefer adding new fields to `manifest.yaml` (with sensible defaults) over introducing new top-level files.

**Protocol changes (slow).** The heartbeat JSON is versioned (`v`). Adding optional fields is a minor version bump; changing the meaning of an existing field is a major bump and needs a parallel migration path because the daemon and the firmware update independently.

Code style for the firmware is plain Arduino-C++ with minimal dependencies; the existing `main.cpp` is the reference for how much structure is appropriate.

## Credits

- **Anthropic** for [claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy) — the BLE protocol, the state machine shape, and the "fork me with your own character" contract.
- **M5Stack** for the [M5Cardputer](https://github.com/m5stack/M5Cardputer) and the PlatformIO + LittleFS pack pattern that makes sprite swapping a two-command operation instead of a toolchain problem.
- **h2zero** for [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino), the only reason a 240 KB firmware can also speak BLE.
- **bitbank2** for [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF), which Phase C depends on.

## License

- **Core code:** MIT License. See [`LICENSE`](LICENSE). If you fork the code, keep the license file intact and please keep the credit line in `src/main.cpp` — it's three lines and it genuinely helps new forkers find the upstream.
- **Assets under `buddies/*`:** Each pack has its own license in its `manifest.yaml`. Check the `license:` field before reusing.

> ⚠️ **Assets are not MIT by default.** The repo *code* is MIT. The `buddies/default/` pack is MIT. But other packs — including `buddies/tartan/` — are under their own terms and may be non-commercial or all-rights-reserved. Redistributing a pack under MIT when its manifest says otherwise is the #1 way this kind of project breaks. When in doubt, open the manifest first.

---

For the full specification — state machine, manifest schema, host daemon wire format, design rationale — read [`docs/DESIGN.md`](docs/DESIGN.md). **Read it before forking** if you want to change anything structural.
