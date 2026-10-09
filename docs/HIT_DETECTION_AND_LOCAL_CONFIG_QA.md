# FIRETEAM alpha: precise zombie hit detection + protected local config

Status: source committed to main; **MSVC/WinUI compilation and in-game results are not verified**.

## Root cause found

Previously the server accepted an enemy hit as soon as LithTech IntersectSegment
struck the zombie's entire OT_MODEL physics bounding box. For zombies configured
collision_mode=model, GetModelAnimUserDims could replace the authored half
extents with a huge animation bounding box (accepted up to 200/300/200 units).
After that, a separate height threshold called almost the entire upper part
of the oversized box a headshot. This caused phantom damage/blood above heads,
beside the mesh, and from different floors.

NOLF2/Jupiter has model/node APIs including GetNodeTransform and model OBB
accessors. The presence of those APIs does not guarantee each imported Combat
Arms LTB contains usable authored hit OBBs. Therefore this version uses a
safe analytic 3D head/torso/legs narrow-phase on the server, with optional
animated Head bone anchoring if present and plausible.

## Changes

1. Zombies still have a coarse physics box for movement, but its half extents
   are limited by authored collision_x/y/z and absolute server maxima:
   normal/assassin no greater than (26,56,26); Tanker (42,80,42).
   Actual values can be smaller if model animation dimensions are plausible.
2. Every hitscan bullet or shotgun pellet must intersect a tight 3D
   ellipsoid (head, torso, legs) before damage or hit feedback. The whole
   physics AABB is NEVER enough. Regions and blood impact location are
   derived from that tighter authoritative hit.
3. When the ray crosses only an overly broad movement box, the zombie
   is excluded and the identical ray is retried. This does NOT grant a
   bullet a free wall penetration: physical walls are checked separately
   before accepting the precise body intersection. Penetration budget is
   only consumed by an actual permitted world penetration.
4. Input ray positions/directions must be finite. Client camera positions
   must remain near actual standing (65) or crouched (42) eye heights.
   The client cannot supply or expand any hit volumes.
5. Bullet visuals (including DEFAULT_PROJECTILE.LTB) are separate from
   the server's authoritative vector intersection. Replacing the bullet
   cosmetic mesh is not the fix for an enormous target AABB.

These proxy volumes are **not yet vertex-perfect or complete CA per-bone OBB
tracking**. Visual-model alignment and animated special positions need
gameplay QA on Cabin Fever before tuning radii for a public alpha.
Keep separate player/AI movement collision, authored skins, and rendering.

## Standalone geometry test

The portable header src/sshell/FireteamHitMath.h has no LithTech dependencies.
Its regression test is tests/test-hit-math.cpp.

On any machine with g++:

    g++ -std=c++11 -Wall -Wextra tests/test-hit-math.cpp -o hit-math-test
    ./hit-math-test

Checks actual ray entry, misses above heads, misses from a separate floor,
lateral grazes, legitimate angled downward hits, range, and zero-size rays.

## No automatic synchronization of weapon and personal data

build.cmd now **seeds only missing files** in BUILT/config for:
- weapons.cfg
- weapon-library.cfg
- loadouts.cfg
- player.cfg (character template, not Player Profile stats)
- session.cfg (map/difficulty/stat-tracking options)

Once present, these files belong to the LOCAL working installation.
They are NOT overwritten on every build.

The dedicated server staging script similarly preserves its own copies.
Weapon Editor and Weapon Import now read/write BUILT/config/weapons.cfg
rather than altering tracked repository config/weapons.cfg. Availability
changes are local in BUILT/config/weapon-library.cfg.

Also, stage-imported-weapons.cmd now stages weapon assets ONLY: it
never overwrites BUILT/config/weapons.cfg from source behind your back.

Local Weapon QA exclusions remain in
BUILT/config/weapon-quarantine.txt and the safety copy under
%LOCALAPPDATA%\FIRETEAM\weapon-quarantine.saved.txt.
Player Profile match data is stored in BUILT/data/matches/*.json,
or BUILT/Dedicated/data/matches/*.json for headless hosts. Builds do not
copy those records into source or overwrite them. Local launcher profiles
remain in BUILT/fireteam-launcher.json, also outside source.

To sync the author's template into the local game on purpose:

    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\sync-local-config.ps1 -Direction ToRuntime -Files weapons.cfg -ConfirmSync

To manually promote verified BUILT weapons into tracked GitHub source:

    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\sync-local-config.ps1 -Direction ToSource -Files weapons.cfg -ConfirmSync

To sync playable weapons from BUILT into a separate dedicated host:

    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\sync-local-config.ps1 -Direction ToDedicated -Files weapons.cfg -ConfirmSync

Each overwrite makes a timestamped backup of the destination. Manual-sync
backups under config/ are Git-ignored.
Without -ConfirmSync, the script refuses to copy anything.
Never put private player stats or local user profiles in repository source.

Note: If an earlier build already wiped an exclusion, source controls cannot
reconstruct the old exclusion. Re-disable the affected gun ONCE, and then test
the build protection again.

## Windows gameplay QA (first build-error only)

1. Close all running clients, launcher and dedicated server.
2. git pull --ff-only
3. build.cmd and, if needed, build-dedicated.cmd
4. Compare timestamps/content of BUILT/config/weapons.cfg and
   weapon-library.cfg before/after; should not change on rebuild.
5. At fixed same-level distance, fire directly into an infected torso:
   damage, blood location and kill counts must match. Aim 12+ inches above
   its head or off its shoulder: no damage, hit or blood.
6. From the second floor, fire horizontally above/beside a zombie below:
   MUST MISS. Aim genuinely downward into it: should damage when unblocked.
7. Test diagonal shots, point-blank, moving animation and Tanker/Assassin;
   they use separate size caps. Check whether headshots are now too strict.
8. Fire past a zombie's oversized broad phase into another zombie or wall:
   first zombie should not shield the ray; walls still block or penetrate
   only under configured rules.
9. Try shotgun 6/8 pellet blast at short/long range, compare damage and
   confirm there are no unexpected extra damage instances. Test a rifle,
   melee and explosive gun for regressions.
10. Disable M16A3 in Weapon QA, rebuild, and verify it stays disabled
    in the Editor/Loadout without manual copying. Repeat after launcher
    Weapon Import. Local stats and player profile should remain intact.

Warning: Linux geometry tests cover only the portable math. They do not
prove that Windows Jupiter headers, shader state, networking, model
skeletons and imported CA collision behave correctly until playtested.
