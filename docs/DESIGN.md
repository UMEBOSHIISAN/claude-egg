# Claude-EGG — Design

> Draft 0.1 — 2026-04-20

## 1. Reading the upstream intent

Before designing anything, look at what the two upstream projects (Anthropic + M5Stack) actually chose, because they each tell you something load-bearing.

### 1.1 What `claude-desktop-buddy` is telling us

- **BLE NUS over a JSON line protocol.** Not HTTP, not MCP, not a serial cable. NUS is the lowest-friction way to have a tiny embedded device listen to a Mac without pairing as a keyboard or opening a local port. The fact that Anthropic picked this for a reference project is a hint: *the buddy is meant to be one of many.* A household should be able to run several of these against one host without conflict.
- **One character, generic, ships with the repo.** The upstream isn't trying to be cute for you. It's trying to be a template. The README explicitly invites forks with custom characters. The contract is: *you bring the art, we bring the plumbing.*
- **Permission-prompt passthrough is a first-class feature.** The protocol carries pending permission requests and the device can answer them with a button press. That tells us Anthropic sees the buddy as a peripheral **control surface** for Claude, not just a status light.

**Takeaway for us:** don't fight the protocol. Keep NUS. Keep the JSON shape compatible enough that a Claude-EGG pack can also be swapped for a regular buddy pack without re-flashing firmware.

### 1.2 What `m5stack/M5Cardputer` is telling us

- **PlatformIO + LittleFS.** Asset swaps don't require a toolchain rebuild. Users can flash character packs with `pio run -t uploadfs` alone. This is the whole reason we can promise "fork the repo, drop sprites, done."
- **NimBLE over ESP-IDF's built-in BLE.** Smaller footprint, easier to run alongside BLE HID if we ever want the pet to *also* type. Don't regress to the native stack.
- **8MB PSRAM, ESP32-S3, 240×135 TFT.** Enough for a handful of animated GIFs resident in memory, not enough for any kind of inference. The research note from 2026-04-19 is explicit: **on-device LLM on this chip is a trap.** The pet has no brain. The Mac has the brain. The pet has a face.

**Takeaway for us:** the Mac daemon does all the thinking (log parsing, aggregation, stage computation). The Cardputer firmware is a renderer with enough local state to animate smoothly between heartbeats.

### 1.3 What the research already rejected

Design constraints from prior research:

- ❌ On-device LLM on the Cardputer itself.
- ❌ LE Audio / LC3 (too early).
- ❌ Installing Cardputer toolchains on the production Mac (env-isolation).
- ❌ Flashing random binaries from M5Burner (supply chain).
- ❌ Having the device write directly to production systems.

All of these apply to this project too. In particular: **the Mac daemon reads Claude logs, never writes to any production system.** It's a read-only tenant of the host.

## 2. System shape

```
┌─────────────────────────────────┐          ┌────────────────────────────┐
│  Mac (host)                     │          │  M5Cardputer (pet)         │
│                                 │          │                            │
│  ┌───────────────────────────┐  │  BLE     │  ┌──────────────────────┐  │
│  │ claude_usage_digest.py    │  │  NUS     │  │ NimBLE NUS server    │  │
│  │  • tails ~/.claude/…jsonl │  │          │  │                      │  │
│  │  • rolls up minutes       │──┼──JSON───▶│  │ state machine        │  │
│  │  • emits heartbeat JSON   │  │ 1/10s    │  │  (stage, mood)       │  │
│  └───────────────────────────┘  │          │  │                      │  │
│                                 │          │  │ char_system          │  │
│  ┌───────────────────────────┐  │          │  │  (LittleFS sprites)  │  │
│  │ launchd agent             │  │          │  │                      │  │
│  │  com.umeboshi.claude-egg  │  │          │  │ display (TFT 240×135)│  │
│  └───────────────────────────┘  │          │  └──────────────────────┘  │
└─────────────────────────────────┘          └────────────────────────────┘
```

The firmware never talks to Anthropic. The daemon never writes anywhere except its own state dir at `~/Library/Application Support/claude-egg/`.

## 3. The state model — what makes it a creature that grows

### 3.1 Life stage (slow axis)

A function of **cumulative lifetime Claude Code active minutes**.

| Stage  | Threshold       | Approx. calendar |
|--------|-----------------|------------------|
| EGG    | 0 – 30 min      | day 0            |
| CHILD  | 30 min – 10 h   | first week       |
| TEEN   | 10 h – 50 h     | first month-ish  |
| ADULT  | 50 h – 300 h    | quarter          |
| ELDER  | 300 h+          | lifer            |

Stage transitions are **permanent and one-way** for a given pet identity. If you want a fresh pet, you start a new buddy profile. This is the one part of the concept that's intentionally un-fun — permanent stages are what make the stakes feel real.

Thresholds are declared in `buddies/<name>/manifest.yaml` so packs can have faster-growing or slower-growing creatures.

### 3.2 Mood (fast axis)

A function of **today's pattern**, recomputed every heartbeat.

| Mood      | Trigger                                                      |
|-----------|--------------------------------------------------------------|
| HAPPY     | 1–4 h today, mostly in daylight hours                        |
| EXCITED   | Active session *right now*, short bursts                     |
| TIRED     | 4–8 h today                                                  |
| GRUMPY    | 8 h+ today, or 3+ days in a row with 6 h+                    |
| SICK      | Any activity after 01:00 local time, 2+ nights in a row      |
| LONELY    | 0 min for 24 h                                               |
| ZEN       | 0 min for 7+ days (pet has accepted its abandonment)         |

SICK and GRUMPY are the interesting ones — they're the feedback channel from `feedback_no_late_night_safety_device_impl.md` and the "深夜活動 3日連続" moon reflect finding, smuggled into a thing that feels like a toy. The pet noticing that you're grinding at 3 a.m. is not a safety device (those have their own rules); it's a pet having a normal reaction to neglect.

### 3.3 The pair

The sprite the device renders is `(stage, mood)`. A pack only needs to ship frames for pairs it supports; missing pairs fall back to the same mood at the previous stage, then to a `default` frame. This keeps fork overhead low — a minimal pack can ship 5 sprites (one per stage) and degrade gracefully.

## 4. Swap surface — the pack format

```
buddies/<name>/
├── manifest.yaml
├── sprites/
│   ├── egg_default.gif
│   ├── child_happy.gif
│   ├── child_tired.gif
│   ├── teen_grumpy.gif
│   └── …
└── lines/
    ├── happy.yaml
    ├── tired.yaml
    ├── grumpy.yaml
    ├── sick.yaml
    └── lonely.yaml
```

### 4.1 `manifest.yaml` schema

```yaml
name: tartan
display_name: タータン
author: UMEBOSHI
license: CC-BY-NC-4.0         # per-pack, not inherited from repo
version: 1
# Stage thresholds in minutes. Override to make a faster or slower creature.
stages:
  egg:   0
  child: 30
  teen:  600
  adult: 3000
  elder: 18000
# Frame mapping. Keys are "<stage>_<mood>". Values are filenames in sprites/.
# "_default" is the catch-all for missing moods.
frames:
  egg_default:   egg.gif
  child_happy:   child_happy.gif
  child_tired:   child_tired.gif
  child_default: child_happy.gif
  teen_grumpy:   teen_grumpy.gif
  teen_default:  teen_happy.gif
  # …
# Lines schema is the same: keyed by mood, falls back to default.yaml.
```

The firmware validates the manifest at boot and refuses to load a pack with a missing required field. The default pack is the schema reference.

### 4.2 `lines/<mood>.yaml`

```yaml
lines:
  - "ちょっと休んだら？"
  - "水飲んだ？"
  - "目ぇしょぼしょぼしてへん？"
weight: 1.0   # relative probability when multiple packs are layered (future)
```

Lines are rotated on a seeded RNG keyed by day, so the same mood doesn't spam the same line.

## 5. Host-side daemon

### 5.1 What it reads

`~/.claude/projects/**/*.jsonl` — Claude Code session transcripts. Each entry has a `timestamp` on user/assistant messages (confirmed 2026-04-20).

### 5.2 What counts as "active"

An **active minute** is any wall-clock minute that contains at least one user or assistant message across all sessions. This naturally collapses multiple parallel sessions (you're one human — you can't use two for real) into one timeline.

### 5.3 State it persists

`~/Library/Application Support/claude-egg/state.json`:

```json
{
  "lifetime_minutes": 12847,
  "last_processed_timestamp": "2026-04-20T14:33:02.118Z",
  "daily": {
    "2026-04-20": { "minutes": 412, "late_night_minutes": 0 },
    "2026-04-19": { "minutes": 380, "late_night_minutes": 45 },
    "2026-04-18": { "minutes": 290, "late_night_minutes": 60 }
  }
}
```

Retention: 30 days of daily breakdown, lifetime minutes forever.

### 5.4 BLE heartbeat payload

```json
{
  "type": "egg.heartbeat",
  "v": 1,
  "lifetime_min": 12847,
  "today_min": 412,
  "active_now": true,
  "late_night_streak": 0,
  "silent_hours": 0,
  "ts": "2026-04-20T14:33:02Z"
}
```

Firmware computes stage from `lifetime_min` and mood from the rest. Stage thresholds come from the manifest on the device side, not from the host, so swapping a pack changes growth curves without restarting the daemon.

## 6. Not in MVP

- Multi-pet merge / trading.
- Pet death. (Explicitly a non-goal. Stages are one-way but the pet never dies. This is a choice — the `feedback_play_lane_protection.md` lesson is that the tool shouldn't become a stress source.)
- Streaks as rewards. Dangerous gamification vector; don't turn Claude usage into a Duolingo-style trap.
- Cloud sync. No.

## 7. Open questions

Resolved:

- ~~Repo public-or-private decision.~~ **Public** as of 2026-04-20. Published at `github.com/UMEBOSHIISAN/claude-egg` after Phase A firmware shipped on the Cardputer. Separate from the `claude-cardputer-buddy` public-strengthening ADR because Claude-EGG is a narrower project (a creature that grows) and doesn't inherit that repo's observation-coupling concerns.
- ~~Reference pack choice.~~ Changed from "shisoko (しそこ)" to **tartan (タータン)**, a frog — chosen because the water-based body (puffing / drying / belly-up) gives stronger visual signal per mood than a plum would, and the 3 a.m. belly-up frame is readable at 240×135 with shapes alone (Phase A).
- ~~Hardware scope.~~ Cardputer-only. M5StickCPlus is explicitly out of scope because the keyboard-based pack switcher and the 56-key QWERTY input path are central to the Phase A UX; a two-button StickCPlus would need a different interaction model entirely.

Still open:

1. Confirm `~/.claude/projects/*/` timestamp coverage — are assistant messages consistently timestamped, or only user messages? This drives whether "active minute" is defined on user messages alone or on any message.
2. Default pack art direction — placeholder blob, ASCII creature, something else obviously-placeholder-but-lovable? Current scaffold has no sprite at all.
3. Does the daemon also expose a CLI (`claude-egg today`) or is BLE the only surface? CLI is cheap and useful offline (especially while traveling without the Cardputer).
4. Pack discovery — fetch a community manifest index over HTTPS, or strictly local / git-based?
5. Pack layering — at what point does the single-pack `buddies/<name>/` format get split into independent sprite / voice / rule axes? Currently the `lines/*.yaml:weight` field is reserved for this future format.

## 8. Directory map of this scaffold

```
claude-egg/
├── README.md                       ← user-facing intro
├── LICENSE                         ← MIT (code only)
├── .gitignore
├── platformio.ini                  ← firmware build config skeleton
├── docs/
│   └── DESIGN.md                   ← this file
├── src/                            ← firmware source (empty skeleton)
│   └── .gitkeep
├── buddies/
│   ├── README.md                   ← pack author guide
│   ├── default/                    ← MIT placeholder pack
│   │   └── manifest.yaml
│   └── tartan/                     ← UMEBOSHI reference pack (frog, CC-BY-NC-4.0)
│       └── manifest.yaml
├── tools/
│   └── claude_usage_digest.py      ← host daemon skeleton
└── data/                           ← LittleFS image staging
    └── chars/
        └── .gitkeep
```
