# Buddy packs

Each subdirectory under `buddies/` is a **character pack** — a self-contained bundle of sprites, dialogue lines, and growth thresholds that makes the pet "yours." A pack is the unit of fork-and-share for this project. The firmware knows how to load any pack that conforms to the layout below; pack authors don't need to touch firmware code.

This document is the authoring guide. For the big-picture design — why packs are shaped the way they are and how they fit into the project's state machine — read [`../docs/DESIGN.md`](../docs/DESIGN.md) §4.

## Layout

```
buddies/<name>/
├── manifest.yaml       # required
├── sprites/            # 240×135 GIFs, one per (stage, mood) pair     [Phase C]
└── lines/              # short dialogue YAMLs, one per mood           [Phase C]
```

Minimum viable pack: `manifest.yaml` alone. A pack that only declares a name, author, license, and stage thresholds is already valid — the firmware falls back to a placeholder renderer for missing sprites and to silence for missing lines.

## What's effective today (Phase A)

Phase A runs a shapes-based renderer that reads one thing from the pack:

- `manifest.yaml:stages` — the minute thresholds for each life stage.

That's it. The Phase A renderer doesn't load sprites or lines yet. A pack you author today will have its growth curve honored immediately, and its art will come online when Phase C ships. You can fork and ship a pack right now; it will not render incorrectly, it will just render as shapes.

Phase C (GIF renderer) turns on the rest:

- `manifest.yaml:frames` — stage/mood → sprite filename mapping.
- `sprites/*.gif` — the actual GIFs on LittleFS.
- `lines/*.yaml` — dialogue strings.

Nothing about the pack format changes between phases. A pack authored during Phase A will keep working through Phase C without changes.

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
2. Replace `sprites/*.gif` with your own art. Keep the filenames that are listed in `manifest.yaml:frames`, or update the mapping to match your own filenames. 240×135 is the native screen size; smaller is fine and will be centered.
3. Edit `lines/*.yaml` with your character's voice. One file per mood; the firmware rotates through lines with a seeded daily RNG so the same mood doesn't repeat the same line in a day.
4. Point the firmware at your pack and flash:
   ```bash
   pio run -e m5cardputer -D DEFAULT_BUDDY=\"<your-name>\" -t upload
   pio run -e m5cardputer -t uploadfs
   ```
   (`uploadfs` is a no-op in Phase A but harmless; it becomes required from Phase C.)
5. On-device, the pack switcher key cycles to your pack. Or set it as the default as shown above.

## Frame mapping

The `frames` section of `manifest.yaml` maps `<stage>_<mood>` keys to GIF filenames in `sprites/`.

```yaml
frames:
  egg_default:       egg.gif
  child_happy:       tadpole_happy.gif
  child_sick:        tadpole_sick.gif
  child_default:     tadpole_happy.gif   # fallback for missing moods
  teen_happy:        frog_happy.gif
  teen_sick:         frog_sick.gif
  teen_default:      frog_happy.gif
  # ...
  elder_default:     pond_sage.gif
```

Three rules:

- **`<stage>_default` is the fallback for that stage.** If the current mood has no specific frame, the firmware falls back to `<stage>_default`, and if even that's missing, to a global placeholder.
- **A pack doesn't have to ship every mood.** A minimal pack can ship five frames (one per stage) and every (stage, mood) combination will render to the same frame as `<stage>_default`. Richer packs distinguish more moods per stage.
- **Stage keys are fixed** (`egg`, `child`, `teen`, `adult`, `elder`). The pack is free to *visually* name its stages anything — `tadpole / frog / toad / pond-sage` in tartan's case, `hatchling / drake / wyrm / ancient` in a hypothetical dragon pack — through the sprite filenames and documentation. The firmware only uses the fixed keys internally.

## Stages and thresholds

```yaml
stages:
  egg:    0
  child:  30      # minutes cumulative
  teen:   600
  adult:  3000
  elder:  18000
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

- [`default/`](default/) — **MIT-licensed generic placeholder.** This is the fork template. The intended workflow is that the first line of your fork's first commit is literally `cp -r buddies/default buddies/<your-name>`.
- [`tartan/`](tartan/) — **タータン (Tartan), UMEBOSHI reference pack.** A frog that puffs up when well-fed, dries out when starved, and turns belly-up with X eyes after two nights of 3 a.m. grinding. **CC-BY-NC-4.0**, non-commercial redistribution only. Shipped as a worked example of what a fully branded character pack looks like — both the art direction and the license choice are representative of how a brand-owning forker should ship.
