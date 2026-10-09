# FIRETEAM hosting, browser, mod handshake and hit confirmation

Status: committed source changes awaiting Windows build and network smoke test.
This is an incremental extension of FIRETEAM, not a replacement for the current
LithTech round controller, stats, launcher or resource layout.

## Implemented in this pass

- Launcher: independent **DEDICATED** navigation tab; server name, max players
  (1-24), port, staged DAT map, difficulty (1-10), first round prep, local stats
  on/off, public/unlisted or private PIN intent, and detected local mod packages.
- Saved presets are local in launcher support storage. Private PINs are NEVER
  persisted. Private starts are blocked until engine admission checks exist.
- Selected mods are visible but server start is blocked with those packages until
  an engine mount/compatibility flow exists. The selector never silently ignores
  them while advertising a modded session.
- The dedicated instance uses its own config/session.cfg, including
  stats_enabled=0/1, and honor is enforced inside StatsManager.
- Launcher **REQUEST STOP** writes Dedicated/data/server-stop.request. The
  dedicated server checks this during Update and shuts down via the same graceful
  path as Ctrl+C. Dedicated must be rebuilt to enable it.
- Browser: compact, fixed-width six-column directory layout inspired by Halo CE,
  using NAME / MAP / MODE / PLAYERS / PING / MODS. Unknown player counts and pings
  are dashes, not fabricated numbers. Existing favorite/direct IP functionality
  remains. This is *not yet* a live ping/query browser.
- Combat: hitscan damage confirmation now sends shooter-only feedback from the
  authoritative server to the client. The client shows a brief X marker centered
  at the aim point independently of queued headshot/kill banners. No blood
  particles or impact decals have been added yet.

## Still required to fulfill the requested long-term experience

### Live browser and network protocol

A small bounded **server query** packet should return protocol version, server
name, current/max players, map, difficulty, game mode, current round,
public/private flag, mod-manifest digest and match phase. Rate limit responses;
show a stable timeout/offline state. Ping must come from a measured response,
never from the directory metadata. Keep favorites, LAN scan, and optional
opt-in public directory separately. Avoid mandatory user accounts.

The directory is an unverified opt-in address book today. Do not claim that
fetching public-servers.json finds running game sessions.

### PIN-protected admissions

A four-digit PIN is only low-strength access control, not true security.
Implement a pre-spawn join admission handshake bound to the current session
with server validation, throttled attempts, proper deny/full responses and
appropriate session secrecy. Joining without a correct PIN must never create
a player object or allow play. Do not implement it as a launcher-only gate or
include a PIN in public directory metadata.

### Mod selection, download consent and compatibility

Manifest v1 for each gameplay mod should include mod ID, semantic version,
content files, per-file SHA-256 digests, byte sizes, source URI and trust level.
The server must advertise an aggregate manifest hash and exact allowed mods,
order and game protocol. The client first compares locally installed content
and displays the differences, sizes, source hosts and trust warnings.

Prompt the player explicitly: \"This server needs N missing/different mods
(total size); download and install?\" Offer **Download & Join** or **Cancel**.
Never download on browsing or server selection alone. Download into a temporary
quarantine directory; validate content lengths, paths and digests, prevent
archive traversal, and apply only approved non-executable mod content. Never
automatically execute DLLs or scripts from community servers. Keep downloads
separate from proprietary Combat Arms assets and the GitHub repository.

A successful download is not enough: verify mounted asset/gameplay hashes
before joining and fail closed on mismatched content. If the mod includes
gameplay/AI rules, enforce authoritative server-side compatibility.

### Host management

Later controls: saved named profiles, console/log view, active player list,
kick/ban/admin actions, map rotation, automatic restart after game over,
mid-round joins, server-side stat retention on disconnect, backup and export,
rate limits, connection diagnostics, LAN discovery, optional router guidance.
Do not expose dormant controls as working settings.

### Physical bullet effects

The initial center-screen marker confirms the hit mechanically, not visually
on the zombie. Follow with locally sourced or originally authored lightweight
blood puffs/decals at confirmed impact points, capped and cleaned up; distinct
headshot cue and adjustable hit-marker intensity. Handle penetration, fire
cadence and multiplayer replication without requiring commercial effect files.

## QA

1. Run `git pull --ff-only`, `build.cmd`, then `build-dedicated.cmd` on
   Windows. This project cannot be considered compiled solely from source review.
2. Confirm DEDICATED tab and numeric validation, save/reopen preset, staged map
   selection, stats toggle and private/modded startup blocking.
3. Start a public/unlisted unmodded server and check custom max player cap,
   port, map and difficulty in console.
4. Verify REQUEST STOP exits gracefully without forced termination.
5. Test direct IP/favorites and browser grid with an empty curated directory;
   unknown columns must not show made-up 0 players or ping.
6. Fire at an infected and a wall. Only confirmed enemy hits produce brief X,
   headshot banners still play, and weapon stats reflect accepted shots/hits.
7. With stats off, verify no match JSON is written by that dedicated instance.
8. Regression: existing smooth zombie navigation and stats checkpoints when
   enabled; weapon loadouts and imported mods not overwritten.

Do not commit proprietary Combat Arms resources.