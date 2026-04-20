# `tools/` — host-side daemon

This directory contains the **host-side daemon** for Claude-EGG. It runs on macOS (Linux/Windows ports are a one-afternoon job if anyone wants them but aren't shipped here), tails Claude Code's session logs, and pushes a heartbeat JSON to the Cardputer over BLE NUS every 10 seconds.

## What's here

```
tools/
├── README.md                   ← this file
├── claude_usage_digest.py      ← the daemon
├── requirements.txt            ← Python dependencies (just bleak)
└── .venv/                      ← your virtualenv (created on first setup, gitignored)
```

## Setup

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

macOS 11+ works out of the box. Python 3.9+ is enough.

On first run macOS prompts your terminal for Bluetooth access; grant it.

## Run

```bash
.venv/bin/python claude_usage_digest.py
```

Logs look like this:

```
21:46:20  INFO    scanning for 'Claude-EGG'...
21:46:30  INFO    connected to 71FFB59E-6A76-75F3-CC6B-75764AAD42EC
21:46:34  INFO    heartbeat: life=9908 today=501 active=True late_streak=2 silent_h=0
21:46:48  INFO    heartbeat: life=9908 today=501 active=True late_streak=2 silent_h=0
```

Ctrl-C stops it. Restarting is safe — the daemon is stateless except for the `state.json` cache at `~/Library/Application Support/claude-egg/state.json`.

## What it reads

`~/.claude/projects/**/*.jsonl` — your Claude Code session transcripts. The daemon only looks at two fields per line:

- `timestamp` — an ISO-8601 UTC timestamp.
- `type` — `user` or `assistant` messages count; everything else is ignored.

Binary dumps inside transcripts (which do happen — Claude logs screenshots and tool output) are skipped via `errors='ignore'` when reading UTF-8.

The daemon never transmits, logs, or persists the content of your prompts or Claude's responses. It counts minutes.

## What it writes

- **Over BLE NUS to the Cardputer**: one heartbeat JSON line every 10 seconds. Schema is documented in the top-level [README.md](../README.md#ble-nus-protocol-phase-b). Never anything else.
- **To local disk**: `~/Library/Application Support/claude-egg/state.json` — a copy of the most recently sent heartbeat. Purely a debugging/observation aid; the pet's state of record lives on the device.

## Definitions

**Active minute.** A local wall-clock minute is "active" if at least one user or assistant message has a timestamp inside it. Multiple parallel sessions within the same minute collapse into one minute (you're one human).

**Today's minutes.** Active minutes whose local date matches today (midnight-to-now in local time).

**Lifetime minutes.** Active minutes ever. Monotonically increasing across days.

**Late-night streak.** The number of consecutive days (ending today or yesterday) that had at least one active minute between 01:00 and 05:00 local time. This is the input to the pet's `SICK` mood. The window and the streak threshold are hardcoded in the daemon; if you want a different schedule, fork the constants at the top of `claude_usage_digest.py`.

**Silent hours.** Hours since the last active minute anywhere. Drives `LONELY` (24 h) and `ZEN` (7 d).

**Active-now.** True if there's been an event in the last 60 seconds. Drives the amber pulse dot on the Cardputer and the `EXCITED` mood.

## Keep it running across reboots

The `examples/com.umeboshi.claude-egg.plist` template ships a launchd agent definition you can drop into `~/Library/LaunchAgents/` and load with `launchctl`. See the [launchd section of the root README](../README.md#run-the-daemon--five-minutes). The agent restarts the daemon automatically if it crashes or if the Cardputer goes away and comes back.

## Tuning

Constants at the top of `claude_usage_digest.py`:

| Constant              | What it does                                       | Default |
|-----------------------|----------------------------------------------------|---------|
| `HEARTBEAT_INTERVAL_S`| Seconds between heartbeats.                        | `10.0`  |
| `ACTIVE_NOW_WINDOW_S` | "Active now" cutoff from the last event.           | `60.0`  |
| `LATE_NIGHT_START`    | Local time window lower bound (for SICK).          | `01:00` |
| `LATE_NIGHT_END`      | Local time window upper bound.                     | `05:00` |
| `DEVICE_NAME`         | BLE advertising name to scan for.                  | `Claude-EGG` |

Changing any of these is a personal fork concern; don't send a PR changing the defaults without discussion.

## Known limitations

- **Full re-scan per heartbeat.** The daemon re-reads every `.jsonl` on every heartbeat. For the scale of a typical Claude Code user this is fine (reading a few megabytes of text every 10 s); for a power user with multiple years of logs, a future revision will cache per-file offsets.
- **Single device.** The daemon connects to the first peripheral it sees advertising as `Claude-EGG`. Running multiple Cardputers against one Mac is a Phase-next problem.
- **macOS-only Bluetooth.** Works on other platforms in principle (bleak is cross-platform), but no one has tried Linux/Windows ports yet.
