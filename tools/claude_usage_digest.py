#!/usr/bin/env python3
"""
claude_usage_digest — Claude-EGG host-side daemon.

Tails Claude Code session transcripts under ~/.claude/projects/*/*.jsonl,
rolls up active minutes for today and lifetime, and pushes a heartbeat
JSON to the Cardputer over BLE NUS every 10 seconds.

Run:

    tools/.venv/bin/python tools/claude_usage_digest.py

On first connect the daemon scans for a peripheral advertising the name
"Claude-EGG". It stays connected for the life of the process, reconnects
automatically on drop, and is safe to kill and restart.
"""
from __future__ import annotations

import asyncio
import datetime as dt
import json
import logging
import pathlib
import signal
import sys
from collections import defaultdict

from bleak import BleakClient, BleakScanner

# ---- constants ------------------------------------------------------------

CLAUDE_PROJECTS_ROOT = pathlib.Path.home() / ".claude" / "projects"

STATE_DIR = pathlib.Path.home() / "Library" / "Application Support" / "claude-egg"
STATE_PATH = STATE_DIR / "state.json"

DEVICE_NAME = "Claude-EGG"
NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  # host → device

HEARTBEAT_INTERVAL_S = 10.0
ACTIVE_NOW_WINDOW_S  = 60.0
LATE_NIGHT_START     = "01:00"
LATE_NIGHT_END       = "05:00"

log = logging.getLogger("claude-egg")


# ---- log scanning ---------------------------------------------------------

def iter_timestamped_events(root: pathlib.Path):
    """Yield datetime (UTC) for every user/assistant message under root."""
    for jsonl in root.rglob("*.jsonl"):
        try:
            with jsonl.open(encoding="utf-8", errors="ignore") as f:
                for line in f:
                    try:
                        d = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    ts = d.get("timestamp")
                    kind = d.get("type")
                    if ts and kind in ("user", "assistant"):
                        try:
                            yield dt.datetime.fromisoformat(ts.replace("Z", "+00:00"))
                        except ValueError:
                            continue
        except OSError:
            continue


def active_minutes_by_day(events) -> tuple[dict[str, set[str]], dt.datetime | None]:
    """Bucket events into (YYYY-MM-DD -> {HH:MM keys}) in *local* time.
    Returns (buckets, last_event_utc) where last_event_utc is the latest
    UTC timestamp seen across all events, or None if empty.
    """
    buckets: dict[str, set[str]] = defaultdict(set)
    last_utc: dt.datetime | None = None
    for ts in events:
        local = ts.astimezone()
        day = local.strftime("%Y-%m-%d")
        minute = local.strftime("%H:%M")
        buckets[day].add(minute)
        if last_utc is None or ts > last_utc:
            last_utc = ts
    return buckets, last_utc


def late_night_streak(buckets: dict[str, set[str]]) -> int:
    """How many consecutive days, ending today-or-yesterday, had any minute
    between LATE_NIGHT_START and LATE_NIGHT_END."""
    def had_late(day: str) -> bool:
        minutes = buckets.get(day, set())
        return any(LATE_NIGHT_START <= m < LATE_NIGHT_END for m in minutes)

    today = dt.date.today()
    streak = 0
    # Allow the streak to start at today OR yesterday, so that "slept past
    # midnight but haven't coded yet today" still counts as an active
    # streak through the whole next day.
    d = today
    if not had_late(d.strftime("%Y-%m-%d")):
        d = today - dt.timedelta(days=1)
    while had_late(d.strftime("%Y-%m-%d")):
        streak += 1
        d -= dt.timedelta(days=1)
    return streak


def _late_minutes_in_day(buckets: dict[str, set[str]], day_key: str) -> int:
    return sum(1 for m in buckets.get(day_key, set())
               if LATE_NIGHT_START <= m < LATE_NIGHT_END)


def build_heartbeat(root: pathlib.Path) -> dict:
    buckets, last_utc = active_minutes_by_day(iter_timestamped_events(root))
    today_key     = dt.date.today().strftime("%Y-%m-%d")
    yesterday_key = (dt.date.today() - dt.timedelta(days=1)).strftime("%Y-%m-%d")

    lifetime_min = sum(len(v) for v in buckets.values())
    today_min    = len(buckets.get(today_key, set()))

    now_utc = dt.datetime.now(dt.timezone.utc)
    if last_utc is None:
        silent_s   = 0
        active_now = False
    else:
        silent_s   = max(0, (now_utc - last_utc).total_seconds())
        active_now = silent_s <= ACTIVE_NOW_WINDOW_S

    return {
        "type":              "egg.heartbeat",
        "v":                 1,
        "lifetime_min":      lifetime_min,
        "today_min":         today_min,
        "today_late_min":    _late_minutes_in_day(buckets, today_key),
        "yesterday_min":     len(buckets.get(yesterday_key, set())),
        "yesterday_late_min":_late_minutes_in_day(buckets, yesterday_key),
        "active_now":        active_now,
        "late_night_streak": late_night_streak(buckets),
        "silent_hours":      int(silent_s // 3600),
        "ts":                now_utc.isoformat().replace("+00:00", "Z"),
    }


# ---- state persistence ----------------------------------------------------

def save_state(hb: dict) -> None:
    try:
        STATE_DIR.mkdir(parents=True, exist_ok=True)
        STATE_PATH.write_text(json.dumps(hb, ensure_ascii=False, indent=2))
    except OSError as e:
        log.warning("state save failed: %s", e)


# ---- BLE loop -------------------------------------------------------------

async def find_device():
    log.info("scanning for %r...", DEVICE_NAME)
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=15.0)
    if device is None:
        log.warning("device %r not found", DEVICE_NAME)
    return device


async def send_heartbeats(client: BleakClient, stop: asyncio.Event):
    """Write a heartbeat JSON to the RX characteristic every interval."""
    while not stop.is_set():
        hb = build_heartbeat(CLAUDE_PROJECTS_ROOT)
        payload = (json.dumps(hb, ensure_ascii=False) + "\n").encode("utf-8")
        try:
            await client.write_gatt_char(NUS_RX_CHAR_UUID, payload, response=False)
            log.info(
                "heartbeat: life=%d today=%d active=%s late_streak=%d silent_h=%d",
                hb["lifetime_min"], hb["today_min"],
                hb["active_now"], hb["late_night_streak"], hb["silent_hours"],
            )
            save_state(hb)
        except Exception as e:
            log.warning("write failed: %s", e)
            return
        try:
            await asyncio.wait_for(stop.wait(), timeout=HEARTBEAT_INTERVAL_S)
        except asyncio.TimeoutError:
            pass


async def run_once(stop: asyncio.Event) -> None:
    device = await find_device()
    if device is None:
        await asyncio.sleep(5.0)
        return
    async with BleakClient(device) as client:
        log.info("connected to %s", device.address)
        await send_heartbeats(client, stop)
        log.info("disconnecting")


async def main_loop() -> int:
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:
            pass
    while not stop.is_set():
        try:
            await run_once(stop)
        except Exception as e:
            log.warning("session error: %s", e)
            await asyncio.sleep(3.0)
    log.info("shutdown")
    return 0


def main() -> int:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s  %(levelname)-7s %(message)s",
        datefmt="%H:%M:%S",
    )
    return asyncio.run(main_loop())


if __name__ == "__main__":
    sys.exit(main())
