# FIRETEAM Gameplay Polish — Pass 1

Status: source change proposed for review. **Windows Win32/MSVC build and in-game multiplayer QA remain required.**

## Player-visible intent

1. **Weapon identity:** legacy scoped weapons with no imported Min/Max perturb now use the intended existing fallback: 90/1000 lateral/up spread when firing from the hip, and 0 when zoomed. Imported perturb values remain authoritative. Unscoped movement interpolation is unchanged; perturb axes are aligned to the actual aim, including steep pitch.
2. **Dodgeable infected melee:** a swing announces itself via the existing attack sound/animation and lands after a data-driven delay. The server revalidates the *original* target's existence, targetability, horizontal range, vertical separation and wall line-of-sight at impact. Moving away or breaking LOS makes the strike miss. No new client-trusted damage messages or copyrighted assets.
3. **Variant identity:** attack wind-up values are 0.28s normal, 0.18s Assassin, 0.55s Crusher/Tanker. These are gameplay-test starting values, **not** claims about original Combat Arms timing. No HP, speed, cooldown or damage values changed. A custom config omitting `attack_windup_seconds` retains immediate timing; wind-ups are capped to 75% of cooldown.
4. **Death/disconnect safety:** a zombie dying during its wind-up cancels its pending attack; a missing/dead/spectating victim cannot take that hit.

## Evidence / verification

- Pure C++11 tests in `tests/test-combat-rules.cpp` cover scoped fallback, imported settings, moving accuracy, invalid config, wind-up bounds, dodged hits, cross-floor misses and invalid geometry.
- Existing `tests/test-hit-math.cpp` is also run in the new portable GitHub Action.
- These portable tests **do not** replace a real Visual Studio x86 build against the local Jupiter SDK, animation verification or a live dedicated-server session.
- Existing gameplay save files, launcher presets, weapon definitions and commercial assets are untouched.

## Focused Windows QA

1. `git pull --ff-only` the review branch; `build.cmd` on Windows with the existing local LithTech dependencies.
2. Solo Cabin Fever on difficulty 4: aim a scoped rifle at a far wall target while *not zoomed*, fire several times, then zoom and repeat. Hip shots should spread markedly more. Repeat while looking steeply up/down; spread should follow aim orientation.
3. Allow a normal infected to begin an attack; immediately back out of melee range or move behind a solid wall. Health should not change. Stand in range for the next swing; exactly one server-authoritative damage event should occur. Inspect logs.
4. Difficulty 6: verify an Assassin is quick to strike but dodgable; at round 8, Crusher wind-up should be perceptibly slower. Confirm that killing a zombie mid-swing cancels its hit and that a player on another floor cannot be struck.
5. In two-player dedicated co-op, dodge/survive on both clients; confirm damage, kills, wave counts, respawn and profile checkpoint data remain synchronized. Repeat with 10+ infected for performance; no claims of 24-player validation until tested.
6. Inspect server log for compile/runtime errors and watch for animation/voice mismatches, invisible hit cues and AI freezing during attack wind-up.

## Follow-up evaluation

Record hit/miss rates at 10m/30m for scoped vs hip-fire, compare dodge success on normal/Assassin/Crusher over repeated rounds, and capture clips for subjective feedback. If Tanker lacks a verified attack animation, map legitimate audio/animation assets before adding more intrusive warnings. Evaluate round pacing, score incentives and enemy roles **after** this baseline feels fair.
