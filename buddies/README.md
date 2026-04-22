# Buddy packs

Each subdirectory under `buddies/` is a **character pack** — a self-contained bundle of sprites, dialogue lines, and growth thresholds that makes the pet "yours." A pack is the unit of fork-and-share for this project. The firmware knows how to load any pack that conforms to the layout below; pack authors don't need to touch firmware code.

This document is the authoring guide. For the big-picture design — why packs are shaped the way they are and how they fit into the project's state machine — read [`../docs/DESIGN.md`](../docs/DESIGN.md) §4.

## Layout

```
buddies/<name>/
├── manifest.yaml       # required
├── sprites/            # 240×101 GIFs, one per (stage, mood) pair
└── lines/              # short dialogue YAMLs, one per mood
```

Minimum viable pack: `manifest.yaml` alone. A pack that only declares a name, author, license, and stage thresholds is already valid — the firmware falls back to the shapes renderer for missing sprites and to silence for missing lines.

## What the firmware reads from a pack

- `manifest.yaml:stages` — minute thresholds for each life stage.
- `manifest.yaml:frames` — stage/mood → sprite filename mapping.
- `sprites/*.gif` — animated GIFs, flashed to LittleFS via `pio run -t uploadfs`.
- `lines/*.yaml` — dialogue strings (renderer not yet wired; declared now for forward compatibility).

## Creating your pack

```bash
# From the repo root.
cp -r buddies/default buddies/<your-name>
```

Then:

1. Edit `manifest.yaml`:
   - `name` — lowercase hyphenated, matches the directory. This is what the firmware uses internally.
   - `display_name` — the UI-facing name. Can be any string, any script.
   - `author` — you. Pick whatever you want to be credited as; this appears in the community list.
   - `license` — see [the license section](#licensing-your-pack) below.
   - `version` — start at 1. Bump on every user-visible change.
   - `stages` — minute thresholds. Defaults are fine for most packs; change them only if you want a faster or slower creature.
2. Replace `sprites/*.gif` with your own art. Keep the filenames listed in `manifest.yaml:frames`, or update the mapping. **240×101 px** is the body area (screen is 240×135 total; header=16 px, footer=18 px are drawn by the firmware). Smaller GIFs are fine and will render at the top-left of the body area.
3. Edit `lines/*.yaml` with your character's voice. One file per mood; the firmware rotates through lines with a seeded daily RNG so the same mood doesn't repeat the same line in a day.
4. Point the firmware at your pack and flash:
   ```bash
   pio run -e m5cardputer -D DEFAULT_BUDDY=\"<your-name>\" -t upload
   pio run -e m5cardputer -t uploadfs
   ```
5. On-device, the pack switcher key cycles to your pack. Or set it as the default as shown above.

## Frame mapping

The `frames` section of `manifest.yaml` maps `<stage>_<mood>` keys to GIF filenames in `sprites/`.

```yaml
frames:
  egg_happy:         egg_happy.gif
  egg_default:       egg_happy.gif      # fallback for missing moods
  tadpole_happy:     tadpole_happy.gif
  tadpole_sick:      tadpole_sick.gif
  tadpole_default:   tadpole_happy.gif
  frog_happy:        frog_happy.gif
  frog_default:      frog_happy.gif
  # ...
  pond-sage_default: pond-sage_happy.gif
```

Three rules:

- **`<stage>_default.gif` is the fallback for that stage.** If the current mood has no matching file, the firmware tries `<stage>_default.gif` next, then falls back to the shapes renderer.
- **A pack doesn't have to ship every mood.** A minimal pack can ship five frames (one per stage) and every (stage, mood) combination will render via the `_default` fallback.
- **Stage keys are fixed** (`egg`, `tadpole`, `frog`, `toad`, `pond-sage`). A dragon pack might rename them visually in docs and sprite filenames, but the `manifest.yaml:stages` keys must match the firmware's internal names.

## Stages and thresholds

```yaml
stages:
  egg:       0
  tadpole:   30      # minutes cumulative
  frog:      600
  toad:      3000
  pond-sage: 18000
```

Minute thresholds. A pack can shorten these for a "pet that grows in a weekend" or lengthen them for a "pet that takes a year." **Stage transitions are permanent and one-way** regardless of thresholds — that's a firmware-level invariant and a pack can't opt out of it.

## Lines

Dialogue is grouped by mood into `lines/<mood>.yaml`:

```yaml
lines:
  - "ちょっと休んだら？"
  - "水飲んだ？"
  - "目ぇしょぼしょぼしてへん？"
weight: 1.0
```

`weight` is reserved for the future pack-layering format (mixing lines from multiple packs); it's a no-op in the single-pack loader but declaring it correctly now means your pack keeps working later.

Keep individual lines short — the display is 240×135 and lines render at `textSize=1`. Assume ~30 characters of headroom per line.

## Licensing your pack

**Pack licenses are not inherited from the MIT grant on the repo code.** The firmware is MIT, the scaffolding you copy from is MIT, but the sprites and lines you ship are yours to license however you want. Declare that choice in `manifest.yaml:license`.

Common choices:

- `MIT` — friendliest for forks. Good default for a pack that's meant to travel.
- `CC-BY-4.0` — requires attribution, allows commercial use. Good for non-branded creative packs.
- `CC-BY-NC-4.0` — requires attribution, non-commercial only. **Recommended for branded characters.** This is what `tartan/` uses.
- `All rights reserved` — you own it, no reuse without asking. Good when the character has commercial value tied to it.

If you're not sure, default to `CC-BY-NC-4.0` — it protects the art while letting people enjoy using it personally.

If your pack redistribution is non-commercial only, declare that explicitly as a second field to remove any ambiguity:

```yaml
license: CC-BY-NC-4.0
redistribution: non-commercial only
```

## Submitting your pack upstream

Open a PR adding your pack to the [community list](#community-packs) below. Criteria:

- The manifest validates against the schema in `../docs/DESIGN.md` §4.1.
- A license is declared.
- At least three stages have custom sprites (otherwise it's just a reskin of `default`).
- The pack is named something that makes sense read alone — avoid names that only make sense in your fork's context.

Reviewers check for: name collisions with existing community packs, obviously miscategorized licenses (MIT on a clearly branded mascot is the usual one), and sprite dimensions / file sizes that would break the display loop. Nothing else — art critique is out of scope.

## Community packs

_Empty. Yours could be first. See the submission criteria above._

## Built-in packs

- [`default/`](default/) — **MIT-licensed default pack.** Ships 35 animated GIFs (5 stages × 7 moods). Also the fork template — copy the directory, drop in your art, flash.
- [`tartan/`](tartan/) — **タータン (Tartan), UMEBOSHI reference pack.** A frog that puffs up when well-fed, dries out when starved, and turns belly-up with X eyes after two nights of 3 a.m. grinding. **CC-BY-NC-4.0**, non-commercial redistribution only.
