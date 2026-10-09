# FIRETEAM: Match checkpoints + playable Assassin / Crusher — Windows QA

Status: **code committed to main; real Win11 build, model rendering and gameplay not yet tested.**

## Build and test once

```bat
git pull --ff-only
build.cmd
```

This build stages optional models/animation banks/skins from ignored
`assets-local/Chars_Files_Updated.zip`. No CA assets are added to GitHub.
A clean build may process the newly added textures, then later unchanged builds
reuse the staged ZIP-entry cache. Running only the launcher update without the
game build is insufficient: both updated client/game configs and server module
must match.

The local asset diagnostic (after build):

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\audit-infected-variants.ps1
```

Expect `infected_assassin` and `infected_tanker` to show **READY**.
The actual game log, from the server/host, should include:

```text
Fireteam specials: Assassin=READY Tanker=READY
Fireteam infected: special infected_assassin ...
Fireteam infected: special infected_tanker ...
Fireteam: ROUND 8 CRUSHER BOSS WAVE - ...
Fireteam: CRUSHER spawned 1/1 on round 8.
```

Missing files, missing configurations, or an unavailable model suppress special
spawns and leave common zombies playing the full round. Send the first relevant
`Fireteam infected:` / `Fireteam specials:` log message if a special is
invisible or does not spawn.

## Checkpoints

The authoritative server owns **one stable match ID / JSON file** per map run:

```text
BUILT/data/matches/match-<id>.json
BUILT/Dedicated/data/matches/match-<id>.json
```

- After round 1 starts, checkpoint approximately every **15 server seconds**,
  even if a wave is taking a long time.
- Also checkpoint on the first controller update **after every round clear**,
  including the final credited killing blow and killed zombie subtype.
- On final squad elimination, atomically overwrite that same record with
  `status=completed`. Crashed/quit matches retain
  `status=in_progress` and their last successfully saved values.
- The launcher **PLAYER PROFILE > REFRESH STATS** includes both in-progress
  and complete matches. It deduplicates by `matchId`, counts interrupted
  matches separately and never interprets an incomplete run as a win.
- Windows `MoveFileExA(REPLACE_EXISTING | WRITE_THROUGH)` preserves the
  previous good JSON if a later write is interrupted. The match files are
  local, unsigned and NOT community leaderboard proof.
- Currently this is strongest for single player / continuously connected
  squad members. Preserving disconnected multiplayer members in later
  snapshots needs a server roster retention pass.

## Crusher / Tanker

Combat Arms wiki identifies the **Crusher / Tanker** as a large, slow,
durable special infected, able to cause poison effects on death. FIRETEAM's
initial boss implementation intentionally **does not** create poison clouds;
no new untested player-damage area is added in this pass.

- Data: `config/infected.cfg [infected_tanker]` and
  `config/characters.cfg [infected_tanker]`
- Verified CA asset paths:
  `VIM_F_NM_DF_TANKER_SH.LTB`,
  `ANI_VI_TANKER_SH.LTB`, blue Tanker head/legs/vest DTX files.
- Base stats before active difficulty scaling: **650 HP, 25 melee damage,
  74 run speed**.
- Same validated navigation/collision/smooth-motion system as existing
  normal infected, with model-authored collision dimensions when valid.
- A boss consumes a normal enemy slot and must be killed for the wave to
  finish. The first boss in a wave broadcasts a 3.5s HUD warning.
- Multiple bosses are spaced across the wave.

### Tanker schedule

| Difficulty | Initial/recurring availability |
|---|---|
| 1 | Round 25, then every 20 rounds; at most one |
| 2 | Round 20, then every 16 rounds |
| 3 | Round 17, then every 12 rounds |
| 4 | Round 14, then every 10 rounds |
| 5 | Round 10, then every 8 rounds; two from round 26 |
| **6** | **8, 12, 15, 18, 20, 22, 23, 24, then every round** |
| 7 | 6, 9, 12, 14, 16, then every round from 18 |
| 8 | 4, 6, 8, then every round from 10 |
| 9 | Every round from 3 |
| 10 | Every round from 3, starting with three bosses |

At difficulty 6: 1 Crusher at round 8/12/15, 2 from round 18, 3
from round 24, and 4 from round 32. Boss counts are capped by the actual
round's configured enemy target.

## Assassin

The provided infection wiki does not list an "Assassin" Fireteam zombie
type. This is **a FIRETEAM-designed fast special variant** based on actual
`VIW_F_NM_DF_ASSASSIN_CH.LTB` and `ANI_VI_ASSASSIN_CH.LTB`
assets supplied by the local CA character archive.

- 100 base HP, 9 melee damage, 210 run speed.
- Randomly appears from round 4 at difficulties 4–6, earlier on hard
  difficulties and later on easy difficulties.
- Spawn chance is 12% per otherwise-common eligible slot on difficulty 5–6,
  rising with difficulty; boss slots take priority.
- Original asset-derived `VLST`, `VLWFR`, `VLRFR` movement animation names
  are assigned; both new animation banks were inspected for these strings.
  The body material ordering and *actual playable GetAnimIndex support*
  still need confirmation from the game log/screenshot.
- Special attack and death-animation names were intentionally left empty
  until validated against the composed runtime LTB objects. This avoids
  putting guessed invalid attack names into live character definitions.
  Normal CA voice-bank sounds are reused until unique special VO is mapped.

## Regression checks

On your first new build, please check all of these in one run:

1. Difficulty 5 Cabin Fever: ordinary infected remain smoothly animated,
   the Assassin appears during mid-rounds and the first Tanker is round 10.
2. Difficulty 6 Cabin Fever: Tanker at round 8, and correct blue head/skin
   appearance. Boss takes much longer to kill, stays outside if too large to
   navigate tight doorways, and counts toward ALIVE.
3. Profile checkpoints: after 20–30 seconds in round 1, confirm a JSON appears
   under `BUILT/data/matches`; exit game before Game Over and refresh Player
   Profile to see that unfinished match included. (The last 15 seconds of
   gameplay may not be saved.)
4. Final match: reach Game Over, confirm that the **same** JSON changes
   `status` to `completed`, no duplicate match record is created.
5. Report missing/invisible textures, T-poses, absent attack/death animation,
   malformed collisions, stuck bosses or any compiler error with logs.

Do not add commercial CA models, ZIPs or DTX sound files to Git.
