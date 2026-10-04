# Fireteam

Standalone co-op FPS built from the 2006 LithTech Jupiter Enterprise **SealHunter** networking sample, targeting a FireTeam-style zombie survival game.

## Development workflow

This project now uses **main as the single working branch** for iterative development.

The local Jupiter workspace is persistent:

1. Run `setup-local.cmd` once to extract the original engine/sample archives.
2. After that, pull/update the repo and normally just run `build.cmd`.
3. `build.cmd` reapplies current source patches and refreshes optional local assets automatically.
4. The existing `BUILT\` folder is preserved so extra test files you add there are not wiped.
5. Use `setup-local.cmd --reset` only when you intentionally want a clean re-extraction.

## Local imports

Put these original archives in `imports\`:

- `release.zip`
- `sealhunter.zip`
- `EngineMissing.zip`

Commercial/extracted game assets remain local and are ignored by Git.

For Cabin Fever testing, put these in `assets-local\`:

- `CABINFEVER.DAT`
- `TEXTURES.zip`
- `FX.zip`
- `RS.zip`

## Current bring-up changes

- Modern Visual Studio 2022 x86 build path.
- Game name is now **Fireteam**.
- Combat Arms Bowie knife player-view/world models are staged from local archives.
- Left click uses `fire_0`; right click uses `fire_1`.
- Bowie `SELECT.WAV` and `FIRE.WAV` are supported.
- LightGroup compatibility follows the NOLF2 baked-light multiplier approach.
- Server-authoritative player health starts at 100 HP with a simple texture-free health bar.
- Hosted connection target raised from 12 to 24 for testing.
- SealHunter's enemy-spawning `AIVolume` separated from imported navigation `AIVolume` objects.
- Cabin Fever `GameStartPoint00` support.
- Imported `Trigger` objects are made inert instead of being incorrectly created as SealHunter world models.
- `run-cabinfever.cmd` directly auto-starts the selected map.
- Cabin Fever runs with an always-flushed `cabinfever-error.log` for crash diagnosis.

## Test

Run:

`build.cmd`

Then:

- `BUILT\run-normal.cmd` - stock sample
- `BUILT\run-cabinfever.cmd` - Cabin Fever

If Cabin Fever crashes, send `BUILT\cabinfever-error.log`.

## Local folder name

The checkout folder can be renamed to `BUILD-GAME` (or anything else). `build.cmd` detects an old CMake absolute path and regenerates only `out\build`; it preserves `.local\` and `BUILT\`.

## Camera

Gameplay now defaults to first person. Press `C` while in-game to toggle back to SealHunter-style third person for testing.

## Current gameplay bring-up

- Friendly fire is disabled. Player melee attacks never damage other players.
- Health is server-authoritative at 100 HP with a simple HUD bar.
- Death currently respawns the player at the map start after 2 seconds.
- Cabin Fever `PoisonGas` volumes are registered as damaging containers and apply a simple client poison tint while the player is inside.
- Cabin Fever `Spawner` objects are now recognized, but bring-up is deliberately capped: only `Spawner_02_01` creates one placeholder `FireteamZombie` so testing cannot reproduce the old mass-spawn crash.
- The placeholder zombie uses the existing HARM guard model temporarily, follows the nearest living player, deals 10 damage per hit, has 40 HP, and can be killed by the Bowie knife.
