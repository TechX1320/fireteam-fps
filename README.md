# Fireteam FPS

Standalone co-op FPS experiment built from the 2006 LithTech Jupiter Enterprise **SealHunter** networking sample, targeting a FireTeam-style zombie survival game.

## v0.0.1 - build baseline

The original SealHunter gameplay now compiles locally with Visual Studio 2022 x86 and produces a runnable `BUILT\` folder.

`setup-local.cmd` automatically applies the small modern-C++ compatibility fixes required by current MSVC.

## v0.0.2 - Cabin Fever bring-up

This milestone starts modifying gameplay:

- Listen-server capacity raised from 12 to 24 connections for testing.
- SealHunter's enemy-spawning `AIVolume` is renamed internally to `ZombieSpawner`.
- Imported `AIVolume` objects are registered as inert navigation placeholders so Cabin Fever cannot turn them into seal spawners.
- `GameStartPoint00` is supported with the original `GameStartPoint0` as fallback.
- Cabin Fever resources can be staged locally without committing commercial assets.

## Build locally

Requirements: Windows + Visual Studio 2022 with **Desktop development with C++** and CMake tools.

1. Clone/check out the branch.
2. Put these original archives in `imports\`:
   - `release.zip`
   - `sealhunter.zip`
   - `EngineMissing.zip`
3. For Cabin Fever testing, put these in `assets-local\`:
   - `CABINFEVER.DAT`
   - `TEXTURES.zip`
   - `FX.zip`
   - `RS.zip`
4. Run `setup-local.cmd`.
5. Run `build.cmd`.
6. Test:
   - `BUILT\run-normal.cmd` - stock SealHunter world
   - `BUILT\run-cabinfever.cmd` - Cabin Fever

The original engine files and extracted commercial assets remain local and are ignored by Git.

## Next

Once Cabin Fever reliably loads and spawns a player, the next milestone is player-model/weapon import followed by the first real zombie actor and wave director.
