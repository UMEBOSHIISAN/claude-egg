#!/usr/bin/env python3
"""
claude_usage_digest — Claude-EGG host-side daemon skeleton.

Reads Claude Code session transcripts under ~/.claude/projects/*/*.jsonl,
rolls up active minutes for today and lifetime, and emits a BLE heartbeat
JSON line to stdout (piped to the BLE bridge in a later revision).

This file is intentionally a skeleton. Implementation lands in the next
play-lane session.
"""
from __future__ import annotations

import datetime as dt
import json
import pathlib
import sys
from collections import defaultdict

CLAUDE_PROJECTS_ROOT = pathlib.Path.home() / ".claude" / "projects"
STATE_DIR = pathlib.Path.home() / "Library" / "Application Support" / "claude-egg"
STATE_PATH = STATE_DIR / "state.json"


def iter_timestamped_events(root: pathlib.Path):
    """Yield (datetime, session_id) for every user/assistant message under root."""
    for jsonl in root.rglob("*.jsonl"):
        try:
            with jsonl.open() as f:
                for line in f:
                    try:
                        d = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    ts = d.get("timestamp")
                    kind = d.get("type")
                    if ts and kind in ("user", "assistant"):
                        yield dt.datetime.fromisoformat(ts.replace("Z", "+00:00")), d.get("sessionId")
        except OSError:
            continue


def active_minutes_by_day(events) -> dict[str, set[str]]:
    """Bucket events into (YYYY-MM-DD -> {minute-keys}). A minute counts as
    active if at least one event lands in it."""
    buckets: dict[str, set[str]] = defaultdict(set)
    for ts, _sid in events:
        local = ts.astimezone()
        day = local.strftime("%Y-%m-%d")
        minute = local.strftime("%H:%M")
        buckets[day].add(minute)
    return buckets


def late_night_minutes(buckets: dict[str, set[str]]) -> dict[str, int]:
    """Count minutes falling between 01:00–05:00 local time per day."""
    out: dict[str, int] = {}
    for day, minutes in buckets.items():
        out[day] = sum(1 for m in minutes if "01:00" <= m < "05:00")
    return out


def build_heartbeat(buckets: dict[str, set[str]]) -> dict:
    today = dt.date.today().strftime("%Y-%m-%d")
    lifetime_min = sum(len(v) for v in buckets.values())
    today_min = len(buckets.get(today, set()))
    # Consecutive days with late-night activity ending today.
    lates = late_night_minutes(buckets)
    streak = 0
    d = dt.date.today()
    while lates.get(d.strftime("%Y-%m-%d"), 0) > 0:
        streak += 1
        d -= dt.timedelta(days=1)
    return {
        "type": "egg.heartbeat",
        "v": 1,
        "lifetime_min": lifetime_min,
        "today_min": today_min,
        "active_now": False,  # TODO: needs last-event-within-N-seconds check
        "late_night_streak": streak,
        "silent_hours": 0,    # TODO
        "ts": dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z"),
    }


def main() -> int:
    events = iter_timestamped_events(CLAUDE_PROJECTS_ROOT)
    buckets = active_minutes_by_day(events)
    hb = build_heartbeat(buckets)
    print(json.dumps(hb, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
