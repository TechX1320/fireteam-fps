# FIRETEAM dedicated hosting (experimental)

FIRETEAM has a separate 32-bit, no-renderer Jupiter server host source under
`src/dedicated/`. It uses Jupiter's `ServerInterface`, not the old NOLF2
MFC server application, and remains isolated from the normal FIRETEAM build.

## Build and run on Windows

1. Run `git pull --ff-only` and `build.cmd` from the repository root.
2. Run `build-dedicated.cmd` (requires the local Jupiter SDK).
3. From `BUILT`, run:

```bat
FireteamDedicatedServer.exe --map CABINFEVER --port 27889 --max-players 24 --name "My FIRETEAM Server"
```

You can also use **SERVERS > START DEDICATED SERVER** in the launcher to
start the default map/port with a separate server console.

This is an experimental adapter to the local Jupiter `server.dll`. It has
**not yet been validated against the user's Windows server.dll and SDK**.
If the dedicated build/host initialization fails, report its exact first
error; the normal `build.cmd` and listen-server launch remain independent.

The server reads its own `BUILT/config/session.cfg`, including difficulty,
and loads staged `Worlds/<MAP>.DAT`. The current directory must contain
`Engine.REZ`, `server.dll`, `rez` and the usual Fireteam config.
Run another copy of BUILT for independent dedicated-server instances so a
local launcher cannot overwrite their session settings.

## Discovery / joining

- The in-launcher **SERVERS** tab supports manually entered IP:port and
  persisted local favorites, without accounts.
- **REFRESH DIRECTORY** loads the curated
  [public-servers.json](../config/public-servers.json) from GitHub.
  These records are *not* automatic live player counts, pings or health checks.
- The engine's current default port is **27889**; a custom port can be entered
  after the address (`example.org:27900`).
- Router/firewall/NAT setup is still the host's responsibility.

## Publishing a server

Submit a server listing through a GitHub issue using the Server Listing
template or provide the information to a project moderator. An approved
maintainer adds it to `config/public-servers.json` as an entry:

```json
{
  "Name": "Example Community Server",
  "Address": "server.example.org:27889",
  "Map": "CABINFEVER"
}
```

The directory is an array of these records. Hosting does not automatically
publish a home IP; listing is opt-in. Do not submit an IP address you are not
authorized to make public. Removing the directory entry does not shut down
the server and does not remove players' independently saved favorites.

LAN discovery, live ping/player counts, verification and a self-registration
master server are later milestones.
