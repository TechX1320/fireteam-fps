# FIRETEAM dedicated hosting — experimental Windows build

FIRETEAM now has a **standalone 32-bit, console/headless Jupiter server**.
It is implemented under `src/dedicated` and compiled separately from the
normal gameplay/launcher build. It is not the old NOLF2 MFC ServerApp; it
reuses the same `ServerInterface` initialization and TCP/IP hosting model.

The integration has been checked against the original Jupiter
`sdk/inc/server_interface.h`, `sdk/inc/ltmodule.cpp` and the original
`NOLF2/ServerApp/ServerDlg.cpp`. **Windows compilation, actual map startup
and client joins are not yet validated.** Do not assume public hosting is ready
until the local smoke test succeeds.

## Setup and build

From the repository directory:

```bat
git pull --ff-only
build.cmd
build-dedicated.cmd
```

Normal `build.cmd` does **not** build the dedicated executable. The extra
command compiles the optional CMake target and stages:

```text
BUILT/
  Lithtech.exe                 # playable client
  config/session.cfg           # client/quick-play only
  rez/                         # original staged assets and object.lto
  Dedicated/
    FireteamDedicatedServer.exe
    server.dll
    Engine.REZ
    LTMsg.dll
    SndDrv.dll
    rez/                       # NTFS JUNCTION to BUILT/rez (no asset copy)
    config/
      session.cfg              # independent server difficulty / prep
      ... other game configs
```

The staging script creates a directory junction. It expects an NTFS/local
Windows filesystem and will **refuse to replace a pre-existing non-junction
`Dedicated/rez` directory**. It does not delete/overwrite the game assets.

Dedicated gameplay settings are separated so launching a normal client can't
overwrite a running server's `session.cfg`. The server-side session file is
preserved on rebuild; other copied game configs refresh from `BUILT/config`.
If you rebuild while a server is running, **stop the server first**.

## Test from command prompt

The simplest first smoke test is:

```bat
run-dedicated.cmd
```

It starts **CABINFEVER, port 27889, 24 slots** and leaves the console visible.
For a different staged map and port:

```bat
run-dedicated.cmd --map BLACKLUNG --port 27890 --max-players 16 --name "Test Server"
```

Or from `BUILT\Dedicated`:

```bat
FireteamDedicatedServer.exe --map CABINFEVER --port 27889 --max-players 24 --name "My FIRETEAM Server"
```

Expected successful output includes:

```text
[OK] Jupiter master interface database registered.
FIRETEAM dedicated server running: CABINFEVER on port 27889 (max 24)
Local client test: 127.0.0.1:27889
```

The dedicated process loads the game's `object.lto`, starts the DAT
world, and updates the server shell without a renderer. Its wave controller
waits for the first player to enter.

Open the ordinary FIRETEAM launcher while the server is running; in
**SERVERS**, enter `127.0.0.1:27889` and select **JOIN SERVER**. Use the same
map on the joining client. Watch the server console for the join and Round 1
countdown. Then test a second computer on the LAN using the server's LAN IP.

The launcher also has **SERVERS > START DEDICATED SERVER**, using the chosen
Quick Play map and difficulty plus name/port fields. It starts the isolated
server process rather than a full local player game.

Use **Ctrl+C** in the server console to stop it gracefully.

## Dedicated server settings

`BUILT\Dedicated\config\session.cfg` supports:

```ini
difficulty=4
first_round_prep=45
cabin_spawn_guard=1
```

The launcher updates this independent file with the chosen difficulty/map
when starting a server. You may also edit it manually before starting.

## Server browser and networking

- Direct IP:port and local favorites work without accounts.
- Public directory comes from `config/public-servers.json` and is opt-in,
  manually curated through a GitHub issue. It does not auto-publish home IPs.
- **Automatic LAN status** is now implemented via dedicated UDP 27888 beacons:
  browser shows nearby host names, maps, difficulty and actual connected slots.
  Stop/restart host after building the new dedicated executable. LAN discovery
  does not require router forwarding but may need Windows Firewall permission.
- **Internet master registration**, Internet-side ping, automatic router port
  mapping (UPnP), CGNAT traversal and relay are **not implemented**. See
  docs/LAN_DISCOVERY_IMPACT_DEATH_QA.md.
- For external players, allow the game's port through Windows Firewall and
  configure router forwarding if required. LAN/loopback testing needs neither
  a cloud relay nor port forwarding.
- Hosting and playing on the **same PC** is supported in the directory layout.
  Validate the actual simultaneous join before advertising it publicly.

## Troubleshooting

1. **Dedicated compile error:** send the **first** MSVC error from
   `build-dedicated.cmd`. Game build is unaffected.
2. **"Unable to load server.dll":** check `BUILT\Dedicated\server.dll`,
   `LTMsg.dll`, and any missing DLL reported by Windows.
3. **"CreateServer failed (X)":** send the numeric Jupiter initialization
   code and the surrounding output.
4. **"LoadBinaries failed":** verify the `rez` junction and
   `BUILT\rez\object.lto`. The first server console error matters.
5. **"StartWorld failed":** verify
   `BUILT\Dedicated\rez\Worlds\<MAP>.DAT` and send the engine error string.
6. **Join failed:** first test `127.0.0.1:27889` on the hosting PC, then
   move to LAN IP, then external address; do not debug router settings before
   local server/player compatibility is proven.

Remaining milestones: validated Windows build, player joins, room stability,
server lifecycle/admin controls, live server discovery and optional NAT
handling. These are deliberately separate from current zombie/weapon changes.
