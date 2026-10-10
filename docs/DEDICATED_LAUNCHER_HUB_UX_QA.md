# FIRETEAM - Dedicated launcher / Hub startup regression QA

Status: source committed, Windows build and runtime validation still pending.

## Root cause from 2026-10-10 report

The uploaded dedicated-host.json showed PublishOnline=true,
AutoConfigureRouter=true, HubUrl="".
The hub address visible in the screenshot was a TEXTBOX PLACEHOLDER,
not a configured URL. DedicatedServerService.Validate(forStart:true)
rejected that combination BEFORE Process.Start, hence no dedicated console.
A passing scripts/test-hub-local.ps1 tests the directory endpoints,
not the launcher's server process and not Internet/NAT reachability.

There was a separate real data-loss bug:
build-launcher.cmd used rmdir /s /q BUILT/Launcher, which deleted
dedicated-host.json and server-favorites.json stored in LauncherPaths.SupportRoot.
This was reproducible on every new build.

## Implemented fixes

1. Dedicated tab now includes USE RUNNING LOCAL HUB and START LOCAL HUB.
   The latter starts a previously published standalone EXE. Launcher's
   local Hub is always bound to HTTP loopback ONLY (127.0.0.1:27890).
   The hub has a Windows EXE via build-hub.cmd already.
2. With Advertise ON and an empty URL, START DEDICATED probes the
   actual loopback /healthz and autofills the Hub URL if healthy.
   It never sends heartbeats to placeholder domains.
3. When no Hub is running, the launcher shows an explicit visible
   error dialog with instructions to start the Hub or turn Advertise OFF.
   It does not silently start LAN-only while an online request was made.
4. Clicking Start automatically saves the local preset BEFORE trying to
   launch; clicking SAVE PRESET also remains supported. A missing game
   binary/invalid Hub setting does not erase the chosen preferences.
5. Dedicated process state is observable: status at top and bottom,
   STARTING / DEDICATED RUNNING button states, PID logged to startup
   diagnostics, immediate-exit warning with exit code, shutdown feedback.
   Router UPnP failure never falsely means the dedicated process failed.
6. Launcher writable state moved to BUILT/data/launcher, outside the
   disposable WinUI DLL/program publish. build-launcher.cmd migrates
   the existing dedicated-host.json, server-favorites.json and previous
   startup.log BEFORE removing BUILT/Launcher.
   Existing stable files take precedence over legacy copies.
7. The original game config/weapons.cfg and player-profile match history
   remain unaffected. The dedicated server still writes only its own
   session.cfg in BUILT/Dedicated/config.

## Windows verification steps

First close any manually started FireteamDedicatedServer.exe, the
launcher, and any other running game server on port 27889.

    git pull --ff-only
    build.cmd
    build-dedicated.cmd
    build-hub.cmd

Optional standalone Hub: double click

    out\hub\win-x64\FireteamHub.exe

Alternatively click START LOCAL HUB in the Dedicated tab, or use
dotnet run while developing. The Hub only handles discovery; you
still need to START DEDICATED to run the actual playable server.

Test A: With Hub running, open launcher DEDICATED. Check Advertise,
leave Hub address EMPTY and click START DEDICATED. It should find
the healthy loopback Hub, fill the address and preserve the preset.
A normal Windows console should appear for the dedicated exe. The
status should identify the process as started and contain its PID.
LAN browser should show CABINFEVER and the selected number of slots.

Test B: Stop both processes. With Advertise still ON and Hub URL
empty/unavailable, click START DEDICATED. Expect an obvious modal
describing why it was blocked, NOT a silent no-op. Disable Advertise
and start again; LAN-only hosting should work.

Test C: Close launcher, open it again. Verify the dedicated name,
map, player count, difficulty, preparation time, Hub selection and
router preference persisted. PIN is intentionally never saved.

Test D: Close the launcher, then run build.cmd AGAIN and reopen.
Your dedicated-host.json and server-favorites.json must remain
under BUILT\data\launcher and not reset to defaults.

Test E: With the game running on the same PC as Hub, visit

    http://127.0.0.1:27890/v1/servers

Expect an active listing if Advertise was ON. A LAN row can take
priority in the browser, so no duplicate HUB row is necessary.
If you manually started a server on the SAME game port first,
a new launcher start should fail with a useful exit/status.

The new startup log path is

    BUILT\data\launcher\Logs\startup.log

## Still NOT implemented/verified

The Hub and LAN advertisement alone don't bypass port forwarding.
UPnP automatic mapping is optional and untested on the user's router;
turn it OFF for the first local Hub test. Hard NAT/CGNAT still needs an
embedded relay or another public endpoint. A localhost Hub entry
must not be mistaken for a reachable public Internet server.

There is no MSVC/WinUI build result in the assistant environment.
Do not report the fixes as runtime-proven until the user builds.
