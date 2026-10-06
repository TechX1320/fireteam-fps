# FIRETEAM Launcher

The launcher is a local C#/.NET 8 WinForms frontend for FIRETEAM.

## Current V1 scope

- Single Player
- Host Multiplayer
- Join Multiplayer by IP
- player name
- Cabin Fever map selection
- stock firearm loadout display
- resolution / windowed mode
- fine mouse sensitivity
- game volume
- native LithTech/NOLF2 gamma
- Mods folder entry point

No online accounts or backend are required.

## Build

From the repository root:

```bat
build-launcher.cmd
```

Output:

```
BUILT\Launcher\FireteamLauncher.exe
```

The launcher searches upward for `Lithtech.exe`, so publishing it under
`BUILT\Launcher` lets it automatically find the game runtime in `BUILT`.

## Next launcher milestones

- data-driven loadout profiles
- Normal / Hard / Extreme gameplay profiles
- server browser
- local mod enable/disable and deterministic REZ load order
- tool path configuration and integration for DEdit, FXed, ModelEdit,
  RenderStyleEditor and REZ/content utilities
- mod project creation / manifests / packaging

Original development-tool redistribution rights must be verified before any
third-party binaries are bundled.
