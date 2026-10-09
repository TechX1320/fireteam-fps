# FIRETEAM - One-click public hosting / live directory / guest identities

Status: architecture and read-only diagnostics, **not implemented Internet NAT mapping, live WAN directory, or a relay**. No remote Windows host tests yet.

## Nonnegotiable player UX
- Host presses **Start Dedicated** in the FIRETEAM launcher.
- No manual router configuration for compatible home routers.
- No required Caddy/Hamachi/Tailscale/third-party tool installed on the player's PC.
- Unlisted/LAN stays private by default. Publishing a public home IP requires explicit opt-in.
- Other launchers see actual running, joinable servers; expired/dead hosts disappear automatically.
- Works offline or LAN even when all FIRETEAM Internet services are unavailable.
- A LAN heartbeat does not constitute a publicly joinable Internet game.

## What the existing code does (audited 2026-10-08)
- Dedicated server: `src/dedicated/FireteamDedicatedServer.cpp` uses Jupiter
  `NETSERVICE_TCPIP`, `NetHost.m_Port` default 27889, plus local UDP
  broadcast/loopback `FTLAN1` packets on discovery port 27888 every ~2s.
- Launcher: `LanServerDiscoveryService` receives LAN advertisements;
  `ServerDirectoryService` currently downloads a **static**
  `config/public-servers.json` file from GitHub. There is no live Internet
  heartbeat, availability probe, or dynamic registration.
- Multiplayer reconnect already uses a random 128-bit client capability
  within one running match. `MSG_CS_PLAYERNAME` takes a user-provided,
  up-to-15-character name. The server does **not** check for another
  connected player with the same name. Two distinct reconnect tickets may
  display identical nicknames, confusing scoreboard/stats.
- NOLF2/Jupiter labels the network service TCP/IP, which does **not**
  establish whether this FIRETEAM build's gameplay datagrams actually use
  TCP or UDP sockets or whether a second game port is allocated. Original
  NOLF2 references UDP 27888-27889, but test this BUILD rather than
  assuming the original ports/protocol.

## Feasibility: distinguish two independent systems

### A. Reachability: getting packets to the dedicated host
1. **LAN/local loopback**: already works by address and advertisement; no
   Internet infrastructure or router mapping needed.
2. **Home router automatic port mapping**: request router's permission via
   Windows UPnP IGD or standards-based PCP/NAT-PMP. No manual forwarding,
   but these protocols rely on router/ISP cooperation and a separate Windows
   Firewall allow rule. Map ONLY observed Jupiter game ports/protocols, NOT
   the LAN discovery port. Never overwrite somebody else's mapping.
3. **Public IPv6 direct (future)**: no IPv4 NAT where supported, but requires
   IPv6-capable Jupiter transport and host firewall admission. This has NOT
   been established for the current engine.
4. **UDP hole punching / ICE**: can reach some NAT configurations when both
   endpoints and their routers cooperate. Requires a reachable Internet
   rendezvous/STUN service to exchange candidate addresses. This needs
   careful Jupiter UDP compatibility work; it cannot be assumed from
   `NETSERVICE_TCPIP` alone.
5. **CGNAT/hard-NAT fallback**: to guarantee an Internet connection when
   direct mappings and punching fail, FIRETEAM must have an actually
   reachable relay server (or someone else's). It can be built into the
   game/launcher, requiring **no player-installed extra program**, but
   its operator pays bandwidth/hosting costs and must guard against abuse.
   A reverse HTTP proxy such as Caddy by itself does not provide arbitrary
   old-game UDP NAT traversal.

### B. Discovery: informing all running launchers
1. Keep LAN broadcast as-is for local instances.
2. Public dedicated hosts opt in to send HTTPS heartbeats every 20-30s to
   a small always-reachable FIRETEAM directory endpoint (separate from game
   packets), including only protocol/build version, map, difficulty, current
   players/capacity, game port, listing name and optional ruleset identifier.
3. Public directory expires entries automatically after ~90s of no
   heartbeat, validates size/rate/authentication, and probes the game
   join/query port BEFORE labeling a listing "joinable".
4. Launchers query this live directory approximately every 10-20s, merge
   with LAN/favorites, and display pending/unreachable states honestly.
   Direct-IP and LAN must work while the directory is offline.
5. Do not put a host's private PIN, reconnect ticket, private/local IPs or
   user profiles into announcements. Don't publish a WAN IP without
   explicit public-listing consent.
6. `githubusercontent.com` static JSON is not a real-time writeable
   registry. A tiny backend is required for truly dynamic public presence.
   It can be FIRETEAM-operated (e.g., HTTPS edge function); **no extra app
   on the PC**. A metadata directory is cheap; a relay is separate and can
   consume significant data bandwidth.

## Username / identity invariants (no account DB needed)
- A display name is NOT an identity and must not authorize reconnect,
  privileges, score, equipment, or historical stats. That is already based
  on server-owned reconnect state and a client capability.
- Add a **server-local**, case-insensitive active display-name uniqueness
  check. Initial nicknames are normalized to a bounded safe ASCII/UTF-8
  policy (current protocol allows 15-byte max). Either reject with a
  user-readable reason or assign a deterministic available suffix;
  choose and test one UX, not silent impersonation.
- Reserve names tied to reconnect tickets for the duration of a match if
  reconnection should restore the same display name while disconnected.
  A different ticket cannot claim a reserved display label in that match.
- Different servers can have the same nickname, and the same person can
  create a new token: **global uniqueness/ownership is impossible without
  an external identity registry or account-like cryptographic identity**.
- Treat possession of the token as a capability, not authentication:
  copying a token can impersonate its offline owner. A future optional
  per-installation signing key pair would strengthen continuity without a
  centralized usernames database.

## Minimum implementation sequence
0. **Measure sockets and router capability.** Run
   `scripts/diagnose-host-connectivity.ps1` on Windows WHILE dedicated runs.
   Script checks UDP/TCP listeners and queries Windows UPnP mapping collection
   READ ONLY; never opens a firewall or router port. Optional
   `-CheckExternalIp` contacts api.ipify.org; compare that IP with
   *router WAN* address to check for possible CGNAT or double NAT.
1. Add an **explicit "Automatic router configuration" toggle** to dedicated
   hosting. On opt-in, request mapping via Windows UPnP first; then test PCP
   and NAT-PMP compatibility. Show mapped/not mapped, and verify reachability
   from outside home LAN. Ensure lease/ownership cleanup, no clobber, and
   continue LAN hosting if mapping fails.
2. Stand up a small FIRST-PARTY opt-in HTTPS presence registry with expiry,
   reachability checks, and keyed heartbeat updates. Replace manual GitHub
   listing for dynamic/public servers, keeping curated static fallback.
3. Validate actual Jupiter packet transport. If suitable, add STUN-based
   direct UDP traversal; build opt-in embedded relay fallback for CGNAT and
   difficult routers. Relay is optional when direct access works.
4. Enforce duplicate-name checks and reconnect name reservation; test name
   case variations, duplicate tickets, ALT+F4, and two PCs with the same name.
5. Multiplayer stress tests: same LAN, different ISP, UPnP-compatible router,
   double NAT/CGNAT, deliberate firewall denial, directory outage, relay
   outage, 8 then 16-24 clients.

## Out of scope until networking is reliable
- Mod negotiation/download/manifest mounts.
- Global ranked leaderboards and centralized login.
- Gameplay scoring overhaul and server-authoritative points
  (the next major feature after multiplayer transport + discovery).

References:
- Microsoft IStaticPortMappingCollection::Add and IUPnPNAT COM APIs:
  https://learn.microsoft.com/en-us/windows/win32/api/natupnp/nf-natupnp-istaticportmappingcollection-add
- PCP RFC 6887: https://www.rfc-editor.org/rfc/rfc6887
- NAT-PMP RFC 6886: https://www.rfc-editor.org/rfc/rfc6886
- ICE RFC 8445 (UDP STUN/TURN): https://www.rfc-editor.org/rfc/rfc8445
