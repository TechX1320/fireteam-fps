# FIRETEAM beta candidate — pass 3 (source review branch)

Baseline: GitHub commit `f44b73bcd785cf2f66be857a1a4d11ba377e652d` (PR #3).
This pass is a **source-only combat experiment**, not a tested LithTech runtime release.
It does not change player saves, network protocol, weapon damage, fire rate, reload timing,
reconnect records, mod assets, or game difficulty. Rebuild both client and dedicated.

## Why the prior build could miss a zombie at ten feet

The authoritative engine first intersects a zombie's movement box. The server then
uses three mathematical ellipsoids inside that box for the actual hit. Previously,
`GetModelAnimUserDims` could permanently shrink the movement box to small dimensions
from the **current idle animation**. This not only made the tight body volumes
small, but also prevented the engine's broad-phase query from reporting a hit on
otherwise visible shoulders and limbs.

Now an animation sample can shrink each authored axis by **at most 10%** (never
increase it); the torso is 78% of the resulting horizontal half-extent, still
strictly inside the validated physics bounds. Zombie head ellipsoids and
world-geometry occlusion continue to be authoritative. This is a hypothesis-led
correction; actual LTB silhouette alignment must be checked in-game. Gun damage
and server-authoritative fire cadence are unchanged.

Server logs now include `Fireteam hitbox audit:` at spawn and an occasional
`Fireteam hit QA:` for rejected broad contacts or implausible client camera origins.

## Zombie animation reality

All current `config/characters.cfg` attack animation keys are empty. The last
build therefore selected idle while waiting to hit. This change:

- Ensures repeated one-shot attack clips actually restart at frame zero;
- Searches the *live model* for the already-used `UMFi` melee name (accepts it
  only if GetAnimIndex resolves on that infected model);
- Checks the locally staged model-string report for `ATTACK`/`ATK`/`MELEE`
  candidates and verifies each one with GetAnimIndex before playing;
- Honors explicit attack_anim1/2/3 definitions before automatic fallback;
- Releases a finished one-shot into looping idle during recovery instead of
  holding the final, crouched frame; and
- Reports `automatic=NOT FOUND` once per variant if there is no supported clip.

**Important:** A validated animation name still needs visual inspection. The
original imported LTB/animation bank isn't part of the public repo, so this
pass does NOT assert that a real swipe is available on all three infected.
If the audit says `NOT FOUND`, provide `BUILT/config/infected-animation-valid.txt`
and the `assets-local/Reports/*-strings.txt` name reports. Do not upload the
commercial LTBs publicly. We can then bind the correct playable clip names
through the mod-editable character config.

## Gun feel / HUD

- A local viewmodel kick now slides the gun slightly down/back on each trigger,
  then settles. Moving adds a subtle sway/bob. These **do not move camera aim,
  change the server ray, or add damage**. Existing `ftreducedfx` disables them;
  weapon QA mode does not apply sway while calibrating offsets.
- Four small ticks illuminate around the reticle for 120ms **only** after a
  server-validated zombie impact. No fake positive marker on click/miss/wall.
  Existing server-created world-space blood is untouched.
- Existing HUD and launcher designs remain unchanged apart from these touches.

## Windows acceptance (mandatory before friend beta)

1. **Build** the branch with `build.cmd` and `build-dedicated.cmd`, preserving
   `BUILT/config`, `BUILT/data`, launcher presets, quarantine, and asset imports.
2. **10ft torso:** stationary target, aim center mass and shoulders with AK-47,
   M4A1, MP5, then one shotgun; fire 30 individually aimed shots per weapon.
   Record accepted vs rejected server hits and compare previous build.
3. **No phantom hits:** aim 12 inches above head, beside body, into the wall
   behind the target and through floors. Must still miss if no penetrable
   material. Shoot a moving zombie at different heights.
4. **Melee:** common/Assassin/Crusher each start an actual playable clip,
   remain committed, and reset instead of crouching indefinitely. Dodging
   before contact makes the attack miss; staying in range takes one hit.
   Kill during windup and verify no delayed damage.
5. **Weapon feel:** check 30/60/120 FPS, ADS/no-ADS, standing and moving,
   semi-auto/automatic, empty clips and reloads. Shot direction must stay
   centered while viewmodel moves; reduced effects and weapon QA disable
   distracting motion. No effect on server damage or crosshair truth.
6. **HUD:** one confirmed hit shows brief ticks; miss and hitting a wall do
   not. Test full-screen, 1366x768 and 2560x1080, scoped optics, low HP,
   round HUD and 2-player dedicated. Ensure no center obstruction.
7. **Data:** rebuild twice with modified weapons, infected timing, launcher
   favorites, weapon quarantine and match files; compare timestamps/hashes.

**Beta criterion:** no repeatable close-range misses on well-aimed center mass;
no phantom wall/floor hits; visible, replayable attacks; stable HUD and saves;
clean client+dedi builds and a genuine 2-player sync session. Do not claim
8/16/24-client load validation from these results.

Rollback: revert to `f44b73b` in the review branch and rebuild, retaining
`BUILT/config` and user data. No save migration is required.
