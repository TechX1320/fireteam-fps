# FIRETEAM - Alpha polish: Weapon QA, shotgun ballistics, impact feedback, profile

Status: committed source on main; **Windows compile and gameplay verification pending**.

## Player Profile and hit feedback

The Player Profile retains its compact Minecraft-inspired labeled statistic rows.
It now uses two equally sized columns at desktop width rather than one narrow
column surrounded by empty space. The selected username, completed/interrupted
match accounting, kills by type and weapon usage still use existing checkpoint
JSON; no statistics data schema was changed.

The shooter now receives only the short, WORLD-SPACE blood burst on confirmed
zombie hits. The center-screen letter X has been removed entirely. The
blood burst is a bounded, texture-independent fallback built on LithTech's
DrawPrim rendering (nine red billboards for up to 0.38 seconds, up to 12
concurrent bursts); renderer state is restored before the remaining HUD.
We have NOT enabled an unverified named NOLF2 ClientFX blood group, and the
burst is not yet visible to other players. Correct original FX/particle
presets can be added when their resource names and staging are verified.

## Shotgun behavior

Previously FIRETEAM treated every imported CA hitscan weapon as ONE straight
ray, despite imported CA metadata including ca_vectors_per_round=6 or 8 for
shotguns. A 15-damage Mossberg behaved like a precise single-hit rifle.

The server now fires separate authoritative pellet traces for each shotgun
trigger pull. Every pellet has its own collision and head/body damage check,
passes through the existing penetration / range multiplier rules, and uses
a cone distribution around the aim vector. A close burst clusters on the
target; longer distances naturally lose pellets due to spread PLUS existing
damage falloff. Each trigger pull still consumes only ONE shell and one shot
in weapon-use statistics, while confirmed hitscan impacts count actual
confirmed pellet hits. Shooter blood feedback is capped to two events per
shotgun trigger to avoid flooding the connection.

Weapon fields:
- ca_vectors_per_round: imported CA authoritative pellet count (6/8)
- pellets: optional explicit override, 1-12
- pellet_spread: optional direction offset, default 0.105

The launcher now transfers pellet metadata into active-slot definitions
when equipping a shotgun. Legacy saved shotgun slots without that metadata
are resolved against matching catalog IDs server-side at load time, so
re-equipping is NOT required for existing users.

Do not claim CA-perfect spread/recoil until the original cone semantics and
pellet patterns are validated. Check the server-authoritative close-vs-far
damage empirically at fixed distances, standing still with the same target.

## Weapon QA / batch library changes

Weapon QA (in-game Q) writes exclusions into
BUILT/config/weapon-quarantine.txt. The launcher now keeps a durable,
automatically refreshed local backup at

%LOCALAPPDATA%/FIRETEAM/weapon-quarantine.saved.txt

This is NOT a GitHub-tracked file and contains only local user choices.
Keeping it under BUILT/Launcher would have been incorrect: build-launcher.cmd
deletes that entire folder on every build.

The runtime file is authoritative when newer; the backup is restored if the
runtime exclusion file is missing. The launcher accounts for legacy 31-char
truncated weapon IDs from the C++ model. QA-disabled definitions remain hidden
from Weapon Editor by default; SHOW QA-DISABLED lets you inspect and explicitly
restore them without losing the quarantine state accidentally.

Weapon Editor now exposes ListView multiple-selection checkboxes with
ENABLE SELECTED and DISABLE SELECTED actions. One operation writes the
launcher-only weapon-library.cfg sidecar once for all selected guns. Explicit
ENABLE SELECTED also removes corresponding QA quarantine keys, if any.
Selection/search positions are preserved when applying changes. Changing
weapon stats by itself does NOT clear QA quarantine.

An exclusion file already deleted before these changes cannot be recovered
without an older local backup. Do not silently regenerate it based on guessing
which weapons failed QA.

## Windows smoke test

From repository root, after closing game and launcher:

    git pull --ff-only
    build.cmd

1. Player Profile: two aligned columns, all previously saved stats visible,
   normal scrollbar behavior, same totals for the existing JSON matches.
2. Hit an infected at short/long range: visible blood coming off the zombie
   where it was struck; NO letter X over the crosshair. Shooting walls produces
   no zombie blood. Headshot kill announcers still work.
3. Equip a stock Mossberg 590 or SPAS-12: 6/8 pellet shell, high close-range
   damage, weaker/less reliable at longer distances. Compare against previous
   build, and verify ONE shell consumed per trigger and per-pellet hits logged.
4. Test a rifle, pistol and melee against zombies to catch any regression in
   accuracy, fire cadence, damage, hit regions, and server-side ammo.
5. Weapon QA: launch QA, press Q on a few weapons; return to Weapon Editor,
   confirm QA-disabled guns are hidden by default and counted. Check
   SHOW QA-DISABLED to see them marked [QA DISABLED].
6. Select 3+ distinct catalog weapons using ListView checkboxes, choose
   ENABLE SELECTED, and ensure all are immediately available in Loadout.
   Disable several similarly; selections should not reset while picking guns.
7. Rebuild launcher (which deletes BUILT/Launcher); verify quarantine choices
   still load via %LOCALAPPDATA%/FIRETEAM. When restoring a QA-disabled weapon,
   ensure it no longer appears in the exclusion file and remains available
   after another build.
8. Test with game and launcher closed while rebuilding. No CI/Windows compile
   results have been obtained by the assistant.

For original NOLF2/CA gore FX, first enumerate actual ClientFX group names,
associated textures, and staged runtime resources. Do not invent a named
PlayClientFX group or commit proprietary asset binaries to GitHub.
