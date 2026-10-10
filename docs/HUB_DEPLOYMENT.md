# FIRETEAM Hub - Local first deployment

Status: source committed to main, but no public DNS, TLS certificate, Lenovo
deployment, Windows compilation, or external multiplayer test is completed.

## What is now in the source

- hub/FireteamHub is an independently deployable ASP.NET Core .NET 10
  directory. It stores active listings in memory with a 90-second timeout,
  caps entries, validates payloads, and keeps private host tokens out of
  public responses. Restarting the hub clears its list temporarily.
- A dedicated host advertises ONLY after --hub-url was explicitly supplied,
  using Windows WinHTTP on a background thread every 25 seconds.
  It sends an immediate offline notice on graceful shutdown.
- The launcher Dedicated tab has two SEPARATE opt-ins: public listing (your
  Internet-observed address becomes public) and automatic UPnP mapping.
  UPnP only tries protocols actually listening on the configured GAME port;
  it never touches LAN discovery port 27888 and never overwrites other devices'
  mappings. UPnP may fail or leave a stale mapping after a crash.
- The Community Servers page loads hub metadata every 20 seconds when its
  URL is configured, with LAN, saved favorites, direct-IP and static curated
  fallback still available. Actual occupancy is included, but the listing is
  marked UNVERIFIED until independent game-port queries are implemented.
- Match display names are now case-insensitively unique and reserved with the
  player's reconnect ticket. Renaming after acceptance is ignored.

## One-click localhost Hub and Dedicated controls

The user-facing launcher now has **START LOCAL HUB** (runs the
self-contained out/hub/win-x64/FireteamHub.exe built with build-hub.cmd)
and **USE RUNNING LOCAL HUB** buttons. Once built, FIRETEAM Hub is a
normal clickable EXE; dotnet run is needed only for development.

If Advertise is enabled but Hub URL is empty, START DEDICATED probes
127.0.0.1:27890/healthz and fills the textbox when a real FIRETEAM Hub
is running. Otherwise it displays a clear error dialog instead of
silently failing. Starting a game server is always a separate step from
starting the directory Hub; the Hub does NOT spawn dedicated game hosts.

Presets and favorites no longer disappear after build.cmd: build-launcher.cmd
migrates legacy files to BUILT/data/launcher *before* deleting the old
WinUI publish folder. Startup logs are now under
BUILT/data/launcher/Logs/startup.log. Game configs and match stats are not
rewritten by this migration.

For this specific troubleshooting sequence, see
docs/DEDICATED_LAUNCHER_HUB_UX_QA.md.

## Quick local test (your existing Windows development PC)

### What the launcher screenshot means

If the Community Servers grid displays FIRETEAM Dedicated / CABINFEVER /
LAN / 0/24, that means LAN discovery is WORKING. The local PowerShell hub
smoke test creates its own temporary TEST_HOST listing and removes it after
success, so a PASS does not leave a server in the hub directory. The hub
never launches game servers; the game must run separately (via START DEDICATED,
or later as a Windows startup task on the Lenovo).

If the browser says "Hub URL not configured", use SERVERS >
LOCAL HUB TEST (while the Hub process is running). This checks
http://127.0.0.1:27890/healthz and saves the known-local endpoint. The
CONNECT HUB button accepts an approved HTTPS address for later deployments.
Neither button advertises your IP automatically; the Dedicated tab still
requires "Advertise server on FIRETEAM Hub" to be switched ON.

The server browser now maintains its existing ListViewItems and only
changes rows when fields actually change. It never clears all rows merely
because a 2-second LAN heartbeat arrived. Selection and scroll location
should remain stable.



Run these two commands in separate terminals:

    dotnet run --project hub\FireteamHub\FireteamHub.csproj

    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-hub-local.ps1

Expected: PASS for heartbeat, occupancy, wrong-key rejection and removal.
Default hub URL is http://127.0.0.1:27890 and is LOOPBACK ONLY.

Then, with old game processes stopped, rebuild:

    git pull --ff-only
    build.cmd
    build-dedicated.cmd

Open the new launcher Dedicated tab. For a local connectivity smoke test,
set Hub address to http://127.0.0.1:27890, and explicitly enable Advertise
on FIRETEAM Hub. Leave UPnP OFF for the first loopback test. Start the
dedicated server and open Community Servers. The browser prefers the LAN
version if a HUB entry has the exact same IP:port; this prevents duplicate
rows. Query http://127.0.0.1:27890/v1/servers directly to verify HUB
registration and actual occupancy. Stop dedicated and the entry should
disappear promptly; without graceful stop it expires after 90 seconds.

If the hub isn't running, public registration fails safely and the game
should remain available by LAN or direct IP. If Advertise is unchecked,
neither public registration nor any IP publication occurs.

## Prepare the Lenovo Tiny as the central hub later

    build-hub.cmd

The published self-contained hub executable will be under out/hub/win-x64.
Copy it to the Lenovo. A separately installed Caddy, Node, Python or .NET
runtime is not required on the player's machine OR on the Lenovo for this
self-contained hub build.

### Public address for the Lenovo's own game server

When the hub and a dedicated game server run on the SAME Lenovo, a loopback
heartbeat normally has address 127.0.0.1. Remote players must NEVER receive
that as an Internet join address. Before publishing the central hub, set
FIRETEAM_HUB_PUBLIC_GAME_HOST on the Lenovo to an operator-controlled PUBLIC
IPv4 address or DNS hostname pointing to your home Internet address:

    $env:FIRETEAM_HUB_PUBLIC_GAME_HOST = "game.example.net"

The hub then substitutes this hostname ONLY for loopback-sourced
registrations, while retaining the actual source address and private
registration key for authentication. No remote player host can supply its
own advertised IP/hostname in heartbeat JSON. This address substitution
does NOT create NAT mappings or prove that the game port works externally.
Do not use private 192.168.x.x / localhost addresses for this public value.

For PUBLIC access, obtain a domain name, valid TLS certificate and an
Internet-reachable endpoint. The built-in Kestrel server can serve HTTPS
directly; no third-party reverse proxy is necessary.

Example PowerShell environment for the central Lenovo (not active by default):

    $env:FIRETEAM_HUB_LISTEN = "http://127.0.0.1:27890;https://0.0.0.0:443"
    $env:ASPNETCORE_Kestrel__Certificates__Default__Path = "C:\secure\hub.pfx"
    $env:ASPNETCORE_Kestrel__Certificates__Default__Password = "<private value>"
    .\FireteamHub.exe

The localhost HTTP listener permits a dedicated game on the SAME Lenovo to
register without routing back through the public Internet. Remote launchers
must use the HTTPS hostname. Do not forward the HTTP test port, install a
fake/untrusted public certificate, or commit the certificate or its password.

A home Internet connection might require a one-time mapping of TCP 443 for
the central hub. CGNAT can prevent even this; then some publicly reachable
endpoint/VPS/tunnel is needed. Community PLAYER hosts do not need an
external tool: the client and native dedicated server already contain their
respective discovery logic.

When a real HTTPS hostname is working, set it in BUILT/config/hub-url.txt
and publish it as the official launcher default in config/hub-url.txt.
The build preserves existing user-local Hub configuration rather than
silently overwriting it.

## Router mapping and Internet reality

FIRETEAM now offers a first attempt at UPnP IGD automatic mapping. It still
needs testing on a compatible router and does NOT guarantee public joins.
- It detects a UDP/TCP socket specifically owned by the running
  FireteamDedicatedServer.exe PID (Windows IP Helper API). A port listened
  on by any OTHER process is never exposed accidentally.
- It requests only detected protocol(s) through Windows NATUPnP.
- It refuses to overwrite an existing mapping for another address/port.
- Request Stop cleans up only mappings created by that launcher, after the
  dedicated process has stopped.
- A router crash, launcher crash, CGNAT or blocked Windows Firewall can
  leave the game unreachable. Static UPnP entries may survive crashes:
  inspect your router mappings if you force-kill the host.
- No PCP, NAT-PMP, ICE/STUN or first-party relay is integrated yet.
- An online presence heartbeat is **NOT** proof a player can join.

The next action after a successful Windows build is to run the read-only
diagnostic on the real host while dedicated is running:

    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\diagnose-host-connectivity.ps1

Redact public/private IPs before sharing the full result. Identify which
Jupiter port/protocol actually listens. Then test on a DIFFERENT Internet
connection (not another client behind the same router). This determines
whether UPnP works or we must prioritize a first-party relay.

## Operational safeguards

- Host identity is generated with Windows CNG, unique to each game port,
  and saved privately under BUILT/Dedicated/data/hub-PORT.identity.
- Registration only accepts HTTPS or explicit localhost HTTP in development.
- The hub pins each host ID and key to its observed remote IP and limits
  listings per IP. Anonymous users can still attempt directory spam; further
  abuse controls and independent reachability probes are required for beta.
- The hub NEVER stores player names, local profile stats, private PINs,
  or player reconnect tickets.
- No accounts or database are required for basic discovery.
- Game servers are NEVER automatically advertised on startup; hosts must
  opt in.
