# Fireteam

Standalone co-op FPS built from the 2006 LithTech Jupiter Enterprise **SealHunter** networking sample, targeting a FireTeam-style zombie survival game.

## Development workflow

Fireteam now keeps its **actual game source in Git** under `src/`.

- `src/cshell/` - client game code
- `src/sshell/` - server/game-object code
- `src/shared/` - shared protocol/helpers
- `src/cres/` and `src/sres/` - resource DLL stubs

The old system that regenerated/modified SealHunter source in `.local` on every build is retired. `build.cmd` compiles `src/` directly.

The `.local` tree is dependency-only:
- Jupiter engine/SDK source
- LithTech runtime files
- original SealHunter REZ content still needed by the sample runtime
- locally staged Combat Arms assets

### One-time migration for existing checkouts

Existing development checkouts that already contain the patched Fireteam source under `.local\imports\sealhunter` should run:

`migrate-source.cmd`

That copies the current source into `src/`, removes the obsolete game-source patch scripts, commits the migration, and pushes it to `main`.

After that, normal development is simply:

1. `git pull --ff-only`
2. edit/pull committed files under `src/`
3. `build.cmd`

Do **not** rerun `setup-local.cmd` for ordinary source changes.

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
- `SND.zip` (or another `SND*.zip`; the largest is selected)
- `UI_Items.zip` (optional CA combat-feedback HUD art)
- `RS.zip`

For Fireteam data research, local-only inputs may also include `GMS.zip`, `Decrypted Attributes_mpgh.net.rar`, and the CA reference binaries documented in `assets-local\README.md`. Current GMS format findings are tracked in `docs\GMS_RESEARCH.md`; run `scripts\inspect-gms.ps1` to reproduce the block/tail analysis.

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

The build now **fails immediately** if `src/cshell/ltclientshell.cpp` is missing and tells an existing checkout to run `migrate-source.cmd`. It never mutates the committed game source.

Then:

- `BUILT\run-normal.cmd` - stock/sample sanity test
- `BUILT\run-cabinfever.cmd` - Cabin Fever / Fireteam test

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
- Cabin Fever `Spawner` objects are recognized, but bring-up is deliberately capped: only outside perimeter `Spawner_01_01N` is active so testing cannot reproduce the old mass-spawn crash.
- That single outside spawner creates three human-scale `FireteamZombie` placeholders as an AI/melee validation step. They use temporary HARM-guard visuals while Combat Arms infected assets are brought online. All other imported spawners remain inert.
- Cabin Fever's map-authored navigation metadata is preserved: `AIRegion`, `AIVolume` fields, and `AINodePatrol` objects load instead of being discarded.
- Infected path through the authored AIVolume network using an NOLF2-inspired volume search and shared-opening gates. Collision prefers the staged CA model's authored animation dimensions. Direct pursuit uses agent-width clearance, stuck enemies temporarily force authored volume paths, local obstruction steering can slide around walls, incomplete volume paths get a short dog-leg recovery waypoint, and floor probing now accepts walkable solid WorldModels such as stairs instead of requiring the root BSP world.
- Cabin Fever now has a temporary development round loop using all discovered `Spawner_01_*` perimeter points: staggered randomized spawns, a max-alive cap, round-clear intermission, and simple increasing DEV counts. These values are explicitly temporary until authentic GMS round data is recovered.
- A 2% bonus crawler easter egg can spawn an original SealHunter seal at a randomized authored cabin player start. Bonus seals do not count toward round completion.
- Bowie melee keeps the Combat Arms 135-unit range during bring-up and can damage `FireteamZombie` objects. The old SealHunter single-OBB assumption is patched structurally so persistent local source trees are upgraded correctly.
- Escape settings now include fine-grained horizontal/vertical mouse sensitivity, game volume, native LithTech/NOLF2 gamma, resolution, windowed/fullscreen, Apply Video, Resume and Quit.
- The stock five-slot test loadout is now conventional firearms: `1 = AK-47`, `2 = Beretta M92FS`, `3 = Bowie`, `4 = Colt 1911A1 MEU`, `5 = L96A1`. Explosive weapons remain possible mod content but are intentionally not part of the stock game until their FX/projectile presentation is mature.
- Weapon definitions live in `config/weapons.cfg`. The server owns ammo, damage, cadence and reload completion; clients request actions and render the selected weapon/HUD.
- The L96A1 has a definition-driven right-mouse scope zoom with an initial FIRETEAM optic overlay. The same config format can support later scoped weapons without hardcoding one rifle.
- `R` requests a server-authoritative reload. The HUD shows the selected weapon and server-synchronized ammo; crosshair visibility follows each weapon definition (the CA Bowie disables it).
- Round state is server-broadcast and the client has `ROUND N BEGIN` / `ROUND N CLEAR` announcements plus a persistent round kill/alive line. Cabin Fever's original `SECTION1.WAV` / `SECTION2.WAV` stings now accompany round begin/clear. The Tab scoreboard is Fireteam-oriented and ranks squad members by infected kills.
- Shooter feedback uses the original CA `HEADSHOT`, `NUTSHOT`, `FIRSTKILL`, `DOUBLEKILL`, `MULTIKILL`, `ULTRAKILL`, `FANTASTIC` and `UNBELIEVABLE` HUD textures when `UI_Items.zip` is present, with text fallback otherwise. Head/Nut hits also get short native CA target-hit sound cues.
- Normal infected presentation is external in `config/characters.cfg`. The default CA virus mapping now uses `VLST` idle, `VLWFR` walk, `VLRFR` run and `VDIE` death; corpses remain visible briefly for the death animation without blocking players. Normal infected now also use the original Cabin Fever `NORMAL` SEEENEMY / ATTACK / DEATH voice bank.
- Hitscan fire now validates the client's real first-person camera origin against the authoritative player body. NOLF2-style reverse-trace penetration is definition-driven; the L96 is the first stock shoot-through test while CA material-specific surface rules are still being mapped.
- Reload completion explicitly reconciles server ammo back to the client HUD, preventing a completed zero-mag reload from visually staying at zero.
- Player damage is real again. The earlier respawn loop was caused by stale PoisonGas hazard code surviving in the local generated source; the build now replaces that function body explicitly. Environmental PoisonGas damage remains disabled until Combat Arms safe/outside volume semantics are reproduced.

## Launcher

A local .NET 8 WinForms launcher now lives under `launcher/FireteamLauncher/`.

Current V1 supports:

- Single Player
- Host Multiplayer (24-player host path)
- Join Multiplayer by IP
- player name
- Cabin Fever
- stock loadout display
- resolution/windowed mode
- fine mouse sensitivity
- game volume
- gamma/brightness
- an initial Mods & Tools page

Build it separately with `build-launcher.cmd`. The launcher is deliberately
local/client-side for now; no account or backend is required.

The client now accepts launcher console variables for `fireteammode`,
`playername`, and `joinip`.

## Loading / model research

- FIRETEAM uses a lightweight NOLF2-style loading render thread around the synchronous world load. It now waits for the render thread to actually enter its loop and guarantees a short visible interval, fixing fast loads that could skip the loading presentation entirely.
- The supplied `ST_M_CHILD.LTB` exposes its animation-name strings directly; LTB-to-LTA conversion is not required just to discover names. Findings are recorded in `docs/CA_MODEL_ANIMATION_NOTES.md`.
- Normal infected now reapply their CA animation/face/head child models after creation using the same `ILTCommon::SetObjectFilenames` pattern used by NOLF2 CAI. A matching `CM_HLMT_NM_VIRUS_HM` head child is staged for the current headless-model compatibility test.

### Weapon Studio

The launcher now has an Open1320-inspired Weapon Studio with a searchable left
catalog, grouped gameplay/view/assets panels, active-slot assignment and
definition-driven penetration/view tuning.

`AUTO IMPORT CA CATALOG` can read decrypted Combat Arms weapon attributes,
match local `Guns.zip` / `GunsHH.zip` assets, append `[catalog.*]`
definitions to `config/weapons.cfg`, and extract matched commercial assets
only into ignored `assets-local/WeaponImports`. Supported firearm/melee
entries can be enabled/disabled; normal builds stage only enabled catalog
imports. The runtime continues to consume the active `[weapon1]` through
`[weapon5]` sections, so any catalog weapon can be copied into a loadout slot.

### Character Studio

The launcher also exposes a Character Studio for body/animation models,
textures/render styles, face attachment/alignment, animation-state names and
infected gameplay/collision values across `characters.cfg` and
`infected.cfg`.

## Combat Arms compatibility work

- Local `CLIENTFX.zip` and `ClientFx.fxd` can replace SealHunter's small FxED database without committing commercial files. Cabin Fever references `Foggy_Vio`, which is present in the supplied Combat Arms ClientFX database.
- Cabin Fever texture references are read directly from the DAT during local staging. Missing flattened paths are aliased to matching extracted textures when the match is unambiguous.
- Character staging now includes the normal infected CA body, shared animation child, virus face, matching virus head child, and additional known infected bodies for later data-driven variants.
- LightGroup compatibility now logs the map-authored StartOn/StartColor state and the client's resolved global/base light values during Cabin Fever bring-up. This is diagnostic only; no arbitrary brightness override is applied.


## Source and licensing

The committed game code is derived from the LithTech Jupiter Enterprise SealHunter sample and is distributed under the GNU GPL v2; see `LICENSE`. Original copyright notices are retained in imported source files.

Combat Arms maps, models, textures, sounds, decrypted attributes, GMS files, and other commercial/extracted data are **not** committed to this repository. Those remain local-only under `assets-local/` or `.local/`.


## Current gameplay presentation notes

- Difficulty is now a 0-10 launcher slider. Level 4 preserves the previous Normal baseline, level 5 fills the Normal-to-Hard gap, level 7 is an expert challenge, and level 10 is the former Nightmare profile / Nightmare++ target.
- Headshot and Nut Shot accolades are now killing-shot-only and use tighter hit regions.
- Combat Arms accolade art is queued instead of stacked, enters oversized, settles to full size, holds, and fades.
- The original CA `ROUNDSTART` texture is used with the existing Cabin Fever round-start sting.
- Optional `ZombieAmbience.wav` plays as a low-volume local loop while in-world.
- `SHADERS.zip` is staged for the upcoming render/effect integration pass; copying the FX files alone does not alter the scene.

- Native Xbox/XInput controller support is now available: analog left-stick movement, right-stick look, LT zoom, RT fire, L3 sprint, A jump, B crouch, X reload, Y weapon cycle, R3 melee, View scoreboard, and Menu tap/hold behavior. Reserved grenade/equipment/D-pad bindings match the target layout for future systems; see `docs/CONTROLLER_LAYOUT.md`.
- The launcher Loadout menu now separates Weapon Loadout, Player Gear, and Weapon Mods. Player Gear establishes backpack/armor/head/face slots; Weapon Mods establishes optic/muzzle/magazine/ammo-stash slots without mixing modification data into base weapon definitions.
