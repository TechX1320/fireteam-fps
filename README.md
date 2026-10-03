# Fireteam FPS

Standalone co-op FPS experiment built from the 2006 LithTech Jupiter Enterprise **SealHunter** networking sample, targeting a FireTeam-style zombie survival game.

## v0.0.1 — build baseline

The first milestone is intentionally boring: compile the **untouched SealHunter gameplay source** locally with Visual Studio 2022 x86 and produce a runnable `BUILT\` folder.

No Cabin Fever, zombie AI, player-limit, or model changes are enabled in this first build gate. That lets us separate modern-compiler problems from gameplay problems.

## Build locally

Requirements: Windows + Visual Studio 2022 with **Desktop development with C++** and CMake tools.

1. Clone this repository.
2. Put these three original ZIPs in `imports\`:
   - `release.zip`
   - `sealhunter.zip`
   - `EngineMissing.zip`
3. Run `setup-local.cmd` once.
4. Run `build.cmd`.
5. If it succeeds, launch `BUILT\run-normal.cmd`.

The original Jupiter/SealHunter files remain local and are ignored by Git.

## Next after the baseline builds

- Raise hosted player count toward 16/24.
- Separate SealHunter's enemy spawner from imported `AIVolume` navigation objects.
- Add `GameStartPoint00`-style spawn compatibility.
- Start Cabin Fever map/resource testing.
