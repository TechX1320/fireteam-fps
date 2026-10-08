# FIRETEAM — decentralized match results, lobby, leaderboards and mod integrity

**Status:** Architecture and phased implementation plan. Do not treat an
unimplemented feature as running. The current in-game scoreboard tracks
legacy kills/lives; richer stats, match files, server signatures, cross-server
leaderboards and actual mod attestation are **not** implemented yet.

**Project constraints:** offline-capable; community-hosted 1–24 player servers;
optional public discovery; no mandatory login, central SQL/database, Caddy,
third-party game host or paid infrastructure; mods are first-class citizens;
locally supplied CA content must not be republished.

## Critical truthfulness/security constraints

- Encryption on a player's own server **does not prevent its administrator
  from editing or fabricating match results**. The admin controls process,
  plaintext, keys and files. Encrypt optional sensitive-at-rest data, but
  not as a score authenticity claim.
- A cryptographic **signature** can prove that a match record came from a
  particular host key and was not changed *after signing*. It **cannot**
  prove that host recorded honest events. Hash-chain logs likewise only
  make later edits apparent to independent parties who saved the prior head.
- A SHA-256 **resource manifest** lets cooperative launchers compare exact
  files; a modified client can lie about what it hashed. Full client-side
  anti-cheat cannot be guaranteed by an open-source user-controlled client.
  The server must independently validate hits, position/movement, ammo,
  fire rate, item grants, health and progression. No trust should be placed
  in client-reported kill or score totals.
- A discoverable community-wide leaderboard needs at least a common
  **discovery/bootstrap mechanism** or preexisting peer contacts. FIRETEAM's
  current GitHub-curated `public-servers.json` can be an *optional static*
  bootstrap, not a gameplay database. Direct IP/favorites/LAN work without it.
  Truly always-available global rankings require voluntary mirrors or some
  persistent hosting; they cannot appear from offline servers alone.

## 1. Match/gameover lifecycle

```text
LOBBY / PREPARATION
  -> ROUND_ACTIVE -> ROUND_CLEAR -> INTERMISSION -> ROUND_ACTIVE
  -> GAME_OVER (freeze snapshot of authoritative counters)
  -> RESULTS (30 seconds; stats, browse players, ready/continue)
  -> WORLD_CHANGE_OR_RETRY
  -> LOBBY / PREPARATION (reset all players, rounds, world objects)
```

- **Single-player (already started):** "R: Play Again" reinitializes the
  current level using Jupiter `ILTClient::StartGame`; ESC opens settings/quit.
  The real result screen is the next milestone.
- **Dedicated host:** one controller controls the 30-second results timer,
  chooses the next map from a saved rotation, commands the Jupiter world
  transition, and pushes the fresh map state to *every connected client*.
  If a rotation has one map, that same DAT must be fully restarted.
  Client-local restart is never allowed to restart a multiplayer server.
- **Mid-round join:** server sends the authoritative current round, progress,
  buffs, loadout/ruleset, and match stats snapshot. The joiner enters as a
  participant with an explicit `joinedRound`; joining late cannot inherit
  other players' kills, points or time survived. Spawn protection is separate.
- Keep existing dead/respawn flows, but ensure `GAME_OVER` doesn't become an
  indefinitely frozen world. Map switches should use engine lifecycle APIs;
  they require a real two-client Windows test before enabling by default.
- Before switching worlds, *atomically finalize and flush* the match record.

## 2. Server-side event accounting

The server emits canonical game events and aggregates them in memory. The
client may request stats, but must never send authoritative totals.

Track per player, per match, per map, and optionally per round:

| Family | Server events / fields |
|---|---|
| Participation | join/leave time, joined round, disconnected, survived seconds, highest round |
| Weapons | accepted trigger pulls, projectiles, pellets, shots per weapon ID, hits per region, ammo consumed, reloads |
| Combat | damage dealt/taken, kills by verified zombie type, assists, headshots, deaths, revive events (when implemented) |
| Powerups | source ID, type, pickups, seconds active, stack count; times use existing *combat-only* clock |
| Team | wave clears, highest round, team deaths, saved allies, shared objectives |
| Score | deterministic rule IDs for point additions/subtractions, per-player totals, team total; never directly client-supplied |

**Definition details:** a shotgun trigger pull differs from pellet count.
Accuracy must declare numerator/denominator (e.g. confirmed hit rays /
valid fired rays). "Most used gun" defaults to accepted trigger pulls,
not time equipped. Save the actual asset/weapon ID, not only a renamed label.
Zombie subtype kills need server-generated subtype IDs and cannot be inferred
reliably from the existing generic infected-kill message without expanding
that server event.

Keep an event ledger/aggregate in the server's own directory:
`BUILT/Dedicated/data/matches/<match-id>.json` with a versioned schema and
private append-only events if diagnostic history is enabled. Write a
temporary file, flush, then rename atomically; recover incomplete games
separately without awarding a finished ranking.

Suggested fields:

```json
{
  "schemaVersion": 1,
  "matchId": "random-128-bit-identifier",
  "serverId": "server-public-key-fingerprint",
  "gameBuild": "build-identifier",
  "status": "completed",
  "mapRotation": ["CABINFEVER"],
  "map": "CABINFEVER",
  "difficulty": 5,
  "roundReached": 8,
  "durationSeconds": 1275,
  "rulesetHash": "sha256-hex",
  "modManifestHash": "sha256-hex",
  "rankingCategory": "community",
  "players": [{
    "playerId": "optional-public-key-fingerprint",
    "displayName": "Player",
    "joinedRound": 1,
    "killsTotal": 53,
    "killsByType": {"infected.default": 53},
    "shotsByWeapon": {"famas": 413},
    "powerupsById": {"bottomless": 2},
    "damageDealt": 0,
    "damageTaken": 0,
    "score": 0
  }]
}
```

Values above are illustrative, not real results; optional player identity
is controlled by the player. Don't store private IPs in public summaries.

## 3. Launcher stats / decentralization

**Your Stats:** read local match receipts cached under
`LauncherData/matches/` and merge by `matchId + serverId`. Show match
history, all-time personal bests, favorite weapon, hits/accuracy, kills by
zombie subtype, powerups, rounds, maps, scores, participation, and filters.

**Lobby Stats:** render a compact game-over summary while connected. The
launcher can present the detailed history; the in-game screen only needs
the match outcome, top players, your highlights, and restart/continue state.

**Server Stats:** each dedicated server owns a *read-only summary/query*
interface and archived match receipts. The launcher refreshes when started
or when a match completes; local/known server data is cached for offline use.

**Community leaderboards:** clients query bookmarked and public servers,
verify the host signatures and receipt digests, then merge/cache known
records. Optional community peers may mirror signed receipts. There is no
single global source of truth, so show `known servers / last synced`, allow
per-server views, and label unresolved conflicts honestly. Offline hosts may
have stale/missing results until reachable or mirrored.

For pseudonymous persistent identity without accounts, the launcher can
generate a local player keypair and prove ownership via a signed nonce.
It is optional and does not protect against creation of extra identities.

## 4. Rulesets, mods and leaderboard separation

Each host publishes a *versioned canonical manifest* containing its engine
version, gameplay config (including `weapons.cfg`, `infected.cfg`,
`difficulties.cfg`, powerups and physics), map identifier/content digest,
ordered REZ/resource mounts, and permitted mods with exact IDs and hashes.

**Important:** validate **resource resolution priority** and unexpected
override files, not just an expected filename in one folder. A malicious
higher-priority `RenderStyles/Default.rsa`, override model/texture or
extra REZ must not shadow the asset the host expected. Hash the content
actually mounted by the engine when possible. Gameplay-critical model
collision bounds belong in the manifest too.

Classify mods by impact:

- **Cosmetic only:** HUD layout, crosshair, sniper reticle, sounds without
  gameplay effects. Server can allow these only where they cannot give
  visibility advantages; e.g. opaque/wallhack renderstyles are NOT cosmetic.
- **Gameplay/data:** weapons, player/zombie models affecting collision,
  renderstyles affecting occlusion/visibility, damage, AI, spawns,
  modifiers, maps. Require exact host-approved manifest compatibility.
- **Untrusted native plugins:** require explicit permission; any arbitrary
  client/server native code is a major integrity and security risk.

Three visible leaderboards:

1. **Personal / Local:** anything goes, includes mods, no trust assertion.
2. **Community — host reported:** signed host-origin records, opt-in;
   per-server rankings plus cross-host aggregation with visible provenance.
3. **Standard ruleset:** require matching pinned baseline manifest and
   server-validated game events. Even here, results from a dishonest host
   are NOT independently verified. Trusted tournament/event rankings can
   optionally require organizer-designated servers or independent witnesses,
   without imposing that governance on everyone else.

Never blend "most points" from heavily modded and unmodded servers without
ruleset/map/difficulty filters. A compatible-but-modded server gets its own
category and leaderboards.

## 5. Signing and at-rest storage

Each dedicated host generates an identity key (e.g. Ed25519 via a *verified*
library available to our supported Windows toolchain). The public key is
exportable, the private key stays on the host. The canonical match JSON,
version, key identifier, and previous local receipt digest are signed at
completion. Other launchers verify the signature *and* preserve the
signer's server identity.

Private-key files may be encrypted at rest with Windows DPAPI where
appropriate, which prevents accidental exposure to unrelated accounts.
It does **not** prevent a machine owner/server admin from generating new
scores or signing invented results.

## 6. File-integrity and runtime enforcement

Pre-join handshake (gradual rollout, opt-in until stable):

1. Host sends a unique nonce, protocol version, required manifest hash,
   mount order and mandatory/optional file list.
2. Launcher builds a local canonical SHA-256 inventory with a metadata
   fast-cache; recompute on change, new joins and selected challenges.
3. For missing/incorrect licensed CA assets, report exactly what local
   files the player must import; never redistribute them from hosts.
4. Require exact gameplay-affecting files; only declared safe cosmetics
   differ. Show a readable incompatibility diff before joining.
5. Client proves possession of its optional profile key; server challenges
   and checks file state, but does not assume a modified client cannot lie.
6. Server continues to enforce shot cadence, ammo, damage, line-of-sight,
   world collision, movement tolerances and score event origins.

Server must never accept a client report like `+5000 points` as authority.
A file check is a compatibility barrier/deterrent, NOT proof of an
unmodified live process. An open-source game cannot perfectly prevent
runtime injection/cheats; do not advertise that it can.

## Proposed development order

- **0 (small immediate usability fix):** R to restart solo after Game Over,
  with visible game-over instructions; don't grant clients MP restart power.
- **1:** game-over 30-second results/intermission state; safe same-map reset
  + multiplayer map rotation after proving Jupiter server world transition.
- **2:** authoritative player/match event counters and atomic local match
  result JSON, visible end-of-game summary, launcher Lobby Stats / Your Stats.
- **3:** persistent dedicated-server identity and signed receipts, launcher
  per-server leaderboard views, optional peer mirroring; no required DB.
- **4:** versioned mod/ruleset/resource manifests, compatibility prompt and
  optional strict pre-join checks; audit engine resource priority.
- **5:** optional opt-in public federation, separate standard/community/
  modded leaderboard browsing and event-specific trusted hosting policies.

Do not block local/offline play on leaderboards, the public directory,
mod downloads, network authentication or any external server. Keep the
launcher's independent dedicated server hosting workflow.
