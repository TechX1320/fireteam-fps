# FIRETEAM — 2026 Modernization Phase 1: Motion + Frame Pacing

This is a **source-level first pass**. It is not an engine replacement or a
new animation bank. Windows/MSVC compilation and game tests are still required.

## Zombie locomotion (server-authoritative)

- `config/infected.cfg`: `update_seconds=0.10` keeps expensive enemy AI
  decisions at roughly **10 Hz**.
- `motion_substeps=3` performs three smaller, collision-aware
  `MoveObject` moves per AI decision (**about 30 Hz**). This reduces
  visible 100 ms movement jumps without making LOS, pathfinding, or steering
  decisions three times as often.
- `motion_substeps=1`: **original motion mode**, for direct A/B comparison.
  Values 2 and 4 request 20 Hz or 40 Hz nominal motion.
- Uses actual elapsed server time for decision cooldowns and corpse lifetime,
  with a hitch cap to prevent dangerously large position jumps.
- Walk/run transitions preserve the normalized current gait position
  instead of restarting each loop at frame zero. This is phase continuity,
  **not** full skeletal crossfade or interpolation of LTB animation keyframes.
- Old collision height, stair stepping, spawn logic and round counts are kept.
  The final-infected recovery remains server-authoritative.

## CPU improvements

- Static authored AIVolumes are collected once after each map load; the map
  load callbacks invalidate the cache before changing worlds.
- Zombies reuse a short-lived (75 ms) snapshot of other living zombie positions
  for crowd steering. This avoids traversing *all* world entities once per
  zombie just to find neighbors.
- Pathfinding itself still searches volume connections on demand. An actual
  cached adjacency graph and spatial grid are separate follow-up tasks.

## Fog/background

- The map's authored fog remains unchanged **unless** enabled fog ends beyond
  `FarZ`. In that case, a 5% fade margin (clamped 96-512 units) is used to
  finish the fade before the engine clips distant geometry.
- Turn the safeguard off with `+FTFogGuard 0` in launcher custom commands.
- Fog is static in `worldpropsclient.cpp`; this guard will not cure camera
  shaking, bad textures, z-fighting, low rendering FPS, skybox seams, or
  animation keyframe stutter. Diagnose those separately.

## Frame pacing logger

Put `+FTPerf 1` in the launcher's custom commands. Every 5 seconds, the
client log prints average FPS, average frame time, worst frame time, and
the number of frames longer than 50 ms. No console spam when disabled.

## One-build A/B test checklist

1. `git pull --ff-only`, then `build.cmd`.
2. Start Cabin Fever, observe walking and running zombies at default
   `motion_substeps=3`.
3. Watch narrow doorways and stairwells. Confirm collision/pathing is still
   correct and the last zombie can be killed.
4. Exit; set `motion_substeps=1` in **BUILT/config/infected.cfg** (or in
   repository config followed by rebuilding, if the build restages files).
   Launch the same map and compare movement. Return to `3` afterwards.
5. Test BLACKLUNG separately with the same motion value.
6. Use `+FTPerf 1` to compare frame pacing and identify background flicker
   that coincides with whole-frame stalls.
7. If a foggy map still pops near the horizon, compare with
   `+FTFogGuard 0` and include the `Fireteam worldprops(client)` log line.
8. Report the **first compiler error** if either game module fails to build.

Do not combine animation model replacements, weapon tuning, lighting redesign,
or world/asset changes in the first motion regression test. This update does
**not** alter `config/weapons.cfg` or any locally staged commercial assets.
