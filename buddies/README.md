# Buddy packs

Each subdirectory is a **character pack**. A pack bundles sprites, dialogue lines, and growth thresholds into a self-describing asset bundle that flashes to the Cardputer's LittleFS partition.

## Layout of one pack

```
buddies/<name>/
├── manifest.yaml       # required
├── sprites/            # GIFs, sized 240×135 or smaller
└── lines/              # YAML dialogue files, one per mood
```

See [`../docs/DESIGN.md`](../docs/DESIGN.md) §4 for the full manifest schema.

## Creating your pack

1. `cp -r buddies/default buddies/<your-name>`
2. Replace `sprites/*.gif`. Keep filenames listed in `manifest.yaml`, or update the mapping.
3. Edit `lines/*.yaml` with your character's voice.
4. Fill in `manifest.yaml`:
   - `name` — lowercase-hyphenated, matches the directory
   - `author` — you
   - `license` — pick one. See note below.
   - `version` — start at 1
5. Build + flash:
   ```
   pio run -t uploadfs -e m5cardputer
   ```
6. On-device, press `b` to cycle to your pack. (Or set `DEFAULT_BUDDY=<your-name>` in `platformio.ini` and re-flash firmware.)

## Licensing your pack

Pack licenses are **not inherited** from the MIT grant on the repo code. Pick one and declare it in `manifest.yaml`. Common choices:

- `MIT` — friendliest for forks.
- `CC-BY-4.0` — requires attribution, allows commercial use.
- `CC-BY-NC-4.0` — requires attribution, non-commercial only. Good for branded characters.
- `All rights reserved` — you own it, no reuse without asking.

If you're not sure, default to `CC-BY-NC-4.0` — it protects the art while letting people still enjoy using it.

## Submitting your pack upstream

Open a PR adding your pack to this README's community list below. Criteria:

- Manifest validates.
- License declared.
- At least 3 stages have custom sprites (otherwise it's just a reskin of `default`).

## Community packs

_Empty. Yours could be first._

## Built-in packs

- [`default/`](default/) — MIT-licensed generic placeholder. Use this as a fork template.
- [`tartan/`](tartan/) — **タータン**, UMEBOSHI reference pack. A frog that puffs up when well-fed and turns belly-up at 3 a.m. CC-BY-NC-4.0. Ships as the worked example of a branded character.
