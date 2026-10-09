# FIRETEAM — Player Profile, squad statistics and infected audio (Phase 1)

## Build / playtest

```bat
git pull --ff-only
build.cmd
```

Then test **single-player Cabin Fever difficulty 5**, as before.

1. **Attacks:** an infected reaching you should play a spatial 3D
   `ATTACK1/2/3.WAV` cue with its own 1.10s per-zombie cooldown.
   Alert/SEEENEMY chatter no longer suppresses the first attack cue.
   If it is still silent, look for
   `Fireteam infected audio: PlaySound failed ...` in the server/game log.
   Local SND.zip content is required; no proprietary WAVs are committed.
2. **Reloads:** normal weapon-specific client animations remain unchanged.
   The server refills clips no earlier than 0.125s after a reload request.
   Premature firing attempts during that minimum cause zero authoritative
   damage. This is a minimum, not a promise of perfect anti-cheat.
   Test fast pistols, a slow rifle, automatic reload and Bottomless Mag.
   There should be no duplicate reload animation.
3. **TAB scoreboard:** a large centered panel shows aligned Name / Kills /
   Shots / Hits / Deaths / Powerups / Remaining Lives. No more alignment
   through spaces in a proportional font.
4. **Game Over:** TAB squad results open automatically. Single-player
   `R` Play Again / ESC shortcuts remain visible below the panel.
5. **After Game Over:** the server atomically writes
   `BUILT/data/matches/match-*.json`. On a headless host, it writes into
   `BUILT/Dedicated/data/matches/`. No client-submitted score totals are
   accepted. No public upload occurs.
6. **Launcher > PLAYER PROFILE:** select the same username as in game, then
   `REFRESH STATS`. The launcher aggregates local completed match files
   for this name: kills, rounds, shots, hitscan hits, headshot kills, deaths,
   damage taken, favorites, zombie types, and powerups.

## Limitations (current implementation)

- **Not an account system yet.** The profile aggregates local matches by
  username (current LithTech name limit: 31 characters). Name collisions and
  renamed players are not resolved. Persistent key identities/signatures and
  cross-server leaderboards are future work.
- **Matches saved at squad elimination.** Early disconnects, incomplete
  matches, all-player quit before Game Over and crashes do not yet create
  complete results. Dedicated map cycling is not implemented.
- Hits are only **server-confirmed direct hitscan impacts** at present.
  Projectile/melee damage, assists and detailed accuracy need separate
  instrumentation. `shotsFired` means server-accepted ranged trigger pulls,
  not pellets or client muzzle flashes.
- Weapon usage uses the slots active when the player was created; hot
  inventory/loadout changes during the match need event-snapshot support.
- New zombie **subtype event IDs are now tracked** per player, but Assassin
  and Tanker are NOT enabled/spawning yet. Their exact skin attachments,
  playable animation names, collision dimensions and attack timing must
  be validated before they become counted boss zombies.
- Results are `source: local-unverified` JSON, intentionally neither
  encrypted nor falsely signed. They cannot be trusted as global ranked
  results until a proper host identity and category policy exist.
- The current 125ms reload floor is intentionally permissive. Even an
  unmodified client may legitimately take longer to finish the animation.
  Do not advertise asset hashes as full anti-cheat; server validation
  remains required.

## New infected readiness test (no gameplay changes)

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/audit-infected-variants.ps1
```

The report `assets-local/Reports/infected-variants-readiness.json`
checks locally staged Normal, Assassin, Tanker body/animation-bank files,
animation-candidate reports and five normal voice WAV paths.

It does **not** export LTBs or claim a playable animation. Next development
step: verify `GetAnimIndex` against a composed Assassin/Tanker object, confirm
skins/collision, and implement the difficulty-based variant/boss pool.
Difficulty 5 design remains Tanker every 8 rounds (1 on round 8, 2 on
round 16), with easier levels rarer and Nightmare++ much more frequent.

## Source safety

- No changes to `config/weapons.cfg` or to smooth movement substeps.
- No commercial CA models, textures or sounds pushed into git.
- Stats schema version 1; future schema changes must be gated and migrated.
- Game and launcher must be built together because `MSG_SERVER_SCORES`
  now carries five additional server-owned counters.
