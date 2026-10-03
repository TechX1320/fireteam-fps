# Fireteam FPS

Standalone co-op FPS experiment built from the 2006 LithTech Jupiter Enterprise **SealHunter** networking sample, targeting a FireTeam-style zombie survival game.

## v0.0.1 — build baseline

The first milestone compiles the original SealHunter gameplay locally with Visual Studio 2022 x86 and produces a runnable `BUILT\\` folder.

`setup-local.cmd` applies a tiny compatibility patch set for old Visual C++ behavior (missing return type/include and legacy for-loop scoping). Gameplay remains unchanged in v0.0.1.

## Build locally

Requirements: Windows + Visual Studio 2022 with **Desktop development with C++** and CMake tools.

1. Clone this repository.
2. Put these three original ZIPs in `imports\\`:
   - `release.zip`
   - `sealhunter.zip`
   - `EngineMissing.zip`
3. Run `setup-local.cmd` once.
4. Run `build.cmd`.
5. If it succeeds, launch `BUILT\\run-normal.cmd`.

The build script can find Visual Studio's bundled CMake even when `cmake.exe` is not in the normal Windows PATH.

The original Jupiter/SealHunter files remain local and are ignored by Git.

## Next

- Raise hosted player count toward 16/24.
- Separate SealHunter's enemy spawner from imported `AIVolume` navigation objects.
- Add `GameStartPoint00`-style spawn compatibility.
- Start Cabin Fever map/resource testing.
