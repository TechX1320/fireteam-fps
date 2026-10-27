# FIRETEAM beta combat gate: hit registration at ALL distances

Baseline: Pass 3 review branch (source code at `c9d9ea4` plus the follow-up beta test document). This pass does **not** retune weapon damage, server-authoritative cadence/ammunition, aim spread, model dimensions, or collision/occlusion filters.

## Verified source-level defects

- The client updates mouse/controller aim in `PollInput()`, sends `MSG_CS_SHOOT` within that same function, and only afterwards runs `CPlayerClnt::Update()` and `CCamera::UpdatePosition()` in the frame's main update. The shot therefore uses the **previous-frame camera pose** while the current frame renders the new crosshair pose. Before constructing the shot request, the client now synchronizes the *current* player look rotation and first-person camera transform. The server still validates the shot origin and resolves hits; the client does not choose a victim or hit point.
- `FTHitMath::RayEllipsoid()` computed a discriminant entirely in 32-bit float. At several thousand world units, terms cancel; numerical sampling reproduced accepted hits just outside a target and misses just inside it. The new code computes the ellipsoid ray discriminant and root in `double`, retaining existing server-clamped radii, hit regions and range checks. This does NOT introduce aim assistance.

## Why the earlier hitbox patch wasn't sufficient

Pass 3 prevented an idle animation's sampled dimensions from shrinking the enemy's broad-phase collision box. That addresses one class of close-range misses. It does not correct outdated client firing transforms, server aim dispersion, moving/lagged enemy interpolation, finite weapon range, occlusion, or long-range mathematical precision. These must remain separate hypotheses until actual in-game telemetry is inspected.

## Positive / negative test matrix (Windows proprietary SDK: NOT RUN here)

1. Build `build.cmd` (with local Jupiter imports), then `build-dedicated.cmd` for two-player tests. Confirm checkout HEAD and stage no confidential assets to Git.
2. On Cabin Fever, fire **single shots** at stationary common zombies with FAMAS/AK-47/M4A1 (if equipped), roughly 10 ft, 25 ft, 50 ft, and long sightline. Record clip, bullets spent, confirmed hits, and server log `Fireteam weapon:` / `Fireteam hit QA:` lines. Use the *same weapon* when comparing previous and new builds. Missing weapon assets are not a reason to reset configs.
3. Move mouse slowly and then rapidly across a stationary zombie; repeat while strafing and while tracking a running zombie. Before this pass, the previous-frame camera direction could explain moving reticle misses. Shots now ought to originate from the current aim orientation (server travel/latency remains).
4. At equivalent visible distance, test standing, crouched, zoomed scoped, and hip-fired scoped weapons separately. Scoped hip fire deliberately has higher dispersion; a crosshair centered on an enemy is not a promise of a hit when the firearm is deliberately inaccurate.
5. **Safety cases:** aim 12 in above a head and just outside shoulder; bullet must miss. Test a wall, closed door, and another floor; no through-geometry damage unless the weapon explicitly permits authored penetration. Test two players (friendly fire must remain disabled), last round in clip, reload, and network latency. Compare local host vs dedicated to identify movement rewind issues.
6. Retest the zombie attack animation regression and Pass 3 HUD / viewmodel recoil. No changes to their data or state are introduced here.

## Checks and data impact

- PASS (run): native portable CMake C++ regression suite, six tests (GCC C++11 and Clang ASan/UBSan) in the patch-authoring environment. Added deterministic far-edge hit and far-miss/range tests.
- REVIEWED (static): frame processing order and exact client shot construction; server retains ammo/shot cadence/geometry authority.
- NOT RUN: full LithTech Win32 build, visual reticle-to-impact comparison, long-range moving target, online dedicated-server, latency/rollback, actual imported weapon/animation assets.
- Persistence: none. No runtime `BUILT/` data, player records, launcher settings or mod configs modified. No wire protocol change.
- Rollback: revert the changes in this single review-branch commit; no save migration required.

If misses remain, gather ~20 sample shots per distance, separate fired vs server-accepted and confirmed hit events. Specifically check reported camera-origin rejection, narrow/cover rejection, `no hit`, `world/solid hit`, and whether the shooter is moving, zoomed or online. Do not solve unresolved misses by globally enlarging hitboxes or claiming hits from client tracers.
