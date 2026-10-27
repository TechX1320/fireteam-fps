new('docs/GAMEPLAY_POLISH_PASS2.md', r"""# Gameplay polish pass 2 - readable, fair combat

Review branch: `gameplay/combat-feel-melee-aim-20261027`, PR #3.
Baseline: `b38eb02a006d6c716339e392db2960324580ea37` (includes pass 1).
Native full-game build and visual/gameplay signoff require the locally imported Jupiter SDK and Windows runtime. Portable tests are not a substitute.

## What changes when playing

### Melee has commitment and counterplay

The server now uses a single-consumption, absolute-time attack contact and recovery timeline. A committed attack cannot turn or chase during its contact/recovery window. Dodging the original target out of reach, behind geometry, or to another floor makes the strike miss; the miss still costs recovery time. Death cancels the attack. A disconnected player, recycled object handle or new respawned life cannot inherit a stale strike.

| Type | Windup | Recovery after contact | Existing cooldown |
| --- | ---: | ---: | ---: |
| Common | 0.28 s | 0.22 s | 1.00 s |
| Assassin | 0.18 s | 0.12 s | 0.85 s |
| Crusher/Tanker | 0.55 s | 0.65 s | 1.65 s |

These are proposed FIRETEAM gameplay starting values, not recovered original Combat Arms animation timing. Contact is resolved on the next AI decision at/after its deadline; the existing 10 Hz decision rate introduces up to roughly one decision interval of quantization under normal scheduling. A long hitch consumes at most one contact. Existing walk/run/navigation and HP/damage/speed remain unchanged. Invalid attack-animation names fall back safely; missing special animation assets still need in-game auditing. Reused attack voice is not a new custom boss sound.

Nearest-target hysteresis reduces target switching between similar-distance players: an existing valid target is retained while no competitor is at least about 20% closer. Difficulty caches reset at a new world, avoiding stale multipliers after changing difficulty.

### Reload time is actually authoritative

The old server completed every reload after 0.125 seconds although clients animated `reload=` from the weapon definition. The server and client now use the same authored duration, safely bounded to 0.125-30 seconds. This closes premature server-ammo availability; it is not a weapon damage rebalance. Existing config values and magazines are unchanged. Reload cancellation and Bottomless retain existing rules.

Ammo text distinguishes reloading with remaining time, an empty weapon, reserves requiring reload, and Bottomless (including 0/0). The countdown reflects local presentation using the same configured duration; authoritative ammo still comes from the server. Remote latency/config mismatch may produce a short ammo-sync wait and must be tested.

### Survival information is clearer

- Numeric health and a LOW HEALTH text warning supplement the bar.
- Four short-lived, victim-only edge cues show the horizontal direction of actual zombie damage. God Mode/respawn immunity does not emit a damage cue. No camera shake, fullscreen red wash or center hitmarker is added.
- Round text separates LEFT, ALIVE and INCOMING, and intermission shows the server-provided countdown. `LEFT` includes enemies still awaiting spawn.
- Essential health/round/radar information is drawn after the scope mask so zoom does not hide it.
- Radar size is adjustable from 75% to 175% in launcher Settings. Range and server visibility rules do not change; allies/bosses also use distinct simple shapes.
- Damage feedback and reduced effects toggles are persisted. Reduced effects lowers blood droplets and removes banner zoom/local health flash; numerical health remains legible.

### Feedback stays timely

Accolades use a four-entry recent queue with coalescing and 2.5-second expiry instead of a 16-entry backlog that could display events tens of seconds late. A round-start announcement preempts stale accolades. Blood remains bounded to 12 bursts, remains shooter-only, and uses a fixed world movement basis rather than orbiting its origin as the camera turns. No proprietary asset is added or guessed.

### Rebuilds preserve gameplay edits

`build.cmd` and dedicated staging no longer routinely overwrite existing infected/character/difficulty/powerup configs. Missing files are seeded. The only automatic additive migration inserts missing windup/recovery keys into existing matching infected sections; explicit values (including zero) are retained, and a byte-exact backup is created before atomic replacement. No new enemy section is injected into customized files. UTF-16 files fail migration untouched because the native parser cannot read them safely. `autoexec.cfg` is seeded only when absent.

Weapon catalogs, loadouts, player/session configs, QA quarantine state, match JSON and launcher presets are not reset. Dedicated runtime settings remain independent. To receive other future template changes, review/merge them deliberately rather than resetting the whole config directory.

Both launcher and ESC settings now preserve the extended 11-field preferences, while reading old 7/8-field files. Settings writes use separate temporary paths and atomic replacement. No account or match-stat schema migration is included.

## Build and automated validation

From the existing checkout, first inspect `git status --short`; keep any local modifications. Do not run setup-local, migrate-source, reset --hard or clean -fdx for this update.

```bat
git fetch origin
git switch gameplay/combat-feel-melee-aim-20261027
git pull --ff-only
test-gameplay.cmd
build.cmd
build-dedicated.cmd
```

`test-gameplay.cmd` needs the Visual Studio 2022 Developer Command Prompt/CMake, runs independent Win32 native regression tests and config preservation tests, and runs the linked production settings-service tests when .NET is installed. It does not compile the full engine. `build.cmd` builds the actual native game and launcher with the existing local dependencies; dedicated hosting then needs `build-dedicated.cmd`.

GitHub CI tests portable rules on Linux and Windows Win32, exercises config migration in temporary directories, tests the real C# settings service and publishes the actual WinUI launcher with its PRI resource gate. The public CI does NOT have the proprietary Jupiter SDK and cannot compile or play the complete native game.

Two new server-to-client messages are appended (damage direction and intermission); all existing IDs/payloads are unchanged. Update host/dedicated and every client to this revision. Mixed builds have not been certified; there is no new version-negotiation mechanism in this patch.

## Focused playtest matrix

| Gate | Scenario | Expected result | Negative/regression |
| --- | --- | --- | --- |
| Melee | Cabin Fever difficulty 4, provoke common attack then dodge | Cue begins before damage; staying in reach takes one hit; moving away misses | Kill zombie mid-swing; no later damage. Jump/floor/wall separation cannot create a phantom hit |
| Special roles | Difficulty 6, Assassin in eligible waves, first Crusher round 8 | Assassin commits quickly; Crusher has a readable long swing and punishable recovery | No spinning/chasing during recovery; no freezing after recovery; exact special visuals remain asset QA |
| Reload | Select several existing guns, empty/tactical reload, hold fire, switch weapons | Ammo transfer and firing wait for authored reload, countdown ends correctly | No free instant reload, duplicate reload loop or missing reserve ammo; Bottomless 0/0 still fires |
| Damage cues | Let zombies hit from front/back/sides, rotate camera, then enable immunity | Edge cue agrees with attacker; health is numeric; reduced effects avoids flash | Immune and lethal damage do not leave stale cues; respawn clears cues |
| Waves | Finish a round, join during break, wait into next | LEFT/ALIVE/INCOMING reconcile; countdown ends at next wave | No negative incoming count; game over does not show a new-wave countdown |
| UI | 1366x768 and 2560x1080; scoped/unscoped; radar 75/100/175% | Health/round/ammo readable, radar stays in bounds | No crosshair obstruction or clipping; settings off/reduced survive ESC save and relaunch |
| Feedback | Rapid kills/shotgun, turn quickly during blood burst | Short recent accolade queue; impacts remain at accepted world hit | No blood on misses/through walls; no stale multikill 20 seconds later |
| Persistence | Customize configs and preferences, build twice, refresh profile | Existing values, match files, presets and quarantine survive | Only missing melee keys added once, backup present; dedicated values not overwritten |
| Multiplayer | Two players/dedicated, target switches, disconnect at windup, respawn | Damage/reloads/waves agree on both clients | No old swing damages replacement/new life; existing reconnect behavior and checkpoint continuity |
| Navigation | Cabin stairs, door crowds, BLACKLUNG floor transitions | Existing routes still work; special recovery does not wedge crowds | No through-floor melee or unintended safe spot introduced |

Run a short solo smoke first, then two-player dedicated; four-player late-wave testing precedes any claim of larger-scale readiness. No 24-player, WAN, animation/texture correctness or framerate improvement is claimed.

Server logs now include `Fireteam pacing: round=... seconds=... target=... peak_limit=... bosses=... restores=...`. These are measured round duration and configured limits, not a profiler; they help choose later pacing changes without guessing.

## Verification scope and rollback

Local GCC C++11 tests and Clang AddressSanitizer/UndefinedBehaviorSanitizer tests cover five executables: original ray/ellipsoid and accuracy contracts, melee contact/lifecycle/invalid inputs at five simulated tick rates, presentation/reload math, and recent feedback queue. Read actual CI results before calling Windows/PowerShell/.NET validation a pass. Full native Windows compilation, visuals, real network play and subjective feel remain NOT RUN by the agent.

Rollback source using the previous branch/commit and rebuild both game and launcher; do not delete BUILT or data. Additive config keys are ignored by older code. The extended settings fields are read-compatible with older readers but an older ESC save can drop new preferences. To restore a pre-migration infected file, choose the exact backup under that runtime's config/backups with the game closed. Preserve user edits made after the backup instead of blindly copying it.

## Deliberately deferred

No points economy, grind/currency, new asset pack, nav rewrite, poison-cloud boss damage, population rescale, blanket recoil changes or random weapon-stat retune. Unique Assassin flanks, Crusher area denial, team assists/support, map-authored encounters and richer impact audio should follow recorded playtests and verified asset timings. This pass establishes reliable counterplay/readability and observations for those decisions.
""")
edit('launcher/FireteamLauncher.WinUI/MainWindow.Layout.cs', '7b346b6043219b6113e4665cb132114ad4fdfc87390f1cc754dca16f7051bce0', [
    (88825, 88825, r"""        SetupNumberBox(RadarScaleBox, 0.75, 1.75, 0.05);
        display.Children.Add(LabeledControl("RADAR SIZE (1.00 = NORMAL)", RadarScaleBox));
        DamageFeedbackToggle.OnContent = "Direction + health warning";
        DamageFeedbackToggle.OffContent = "Health bar only";
        display.Children.Add(LabeledControl("DAMAGE FEEDBACK", DamageFeedbackToggle));
        ReducedEffectsToggle.OnContent = "Reduced";
        ReducedEffectsToggle.OffContent = "Normal";
        display.Children.Add(LabeledControl("BLOOD / BANNER MOTION", ReducedEffectsToggle));
"""),
])
