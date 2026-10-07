# FIRETEAM Launcher UX Plan

## Purpose

The launcher is FIRETEAM's primary player-facing shell outside gameplay. It should feel like a compact game frontend and mod workspace, not a generic settings form.

The supplied Open1320 launcher source is a local design/architecture reference only. It is not copied into this repository.

## Design rules

- Brand the product as **FIRETEAM**. Cabin Fever is a supported map, not the launcher title.
- Use a compact **top navigation bar**, not a permanent left rail.
- Keep player tasks separate from developer/content-authoring tasks.
- Use dark graphite surfaces with FIRETEAM orange as the primary accent, cyan as a secondary/system accent, red for warnings, and green only for actual success states.
- Prefer grouped cards/panels with clear headings and one-sentence explanations.
- Avoid giant empty pages and full-width form controls when a compact field is enough.
- Keep gameplay/content data external and data-driven.
- Do not expose a launcher setting until the corresponding LithTech/Jupiter runtime variable is verified.

## Information architecture

### Home
A compact dashboard:
- Quick Play: player name, mode, difficulty, join IP, advanced command line
- Current Loadout summary
- Deploy/status card with Play button
- Right column: News / Updates
- Right column: Build / runtime status

### Play
Top-nav menu:
- Single Player
- Host Multiplayer
- Join Multiplayer
- future Server Browser

Selecting a mode returns to the Home/Ready Room with that mode active.

### Loadout
Combat Arms-inspired player-facing armory:
- three named saved loadouts on the left
- active fighting load across the top
- category tabs: AR, SR, Launcher, Melee, MG, Pistol, SG, SMG, Throwing
- enabled weapon inventory list
- selected-weapon stat inspection
- explicit equip destination
- Primary / Sidearm / Melee / Backpack 1 / Backpack 2
- Save Loadout and Activate Loadout are separate actions
- link to Weapon Editor under Tools

Slot rules:
- Primary: AR / SR / Launcher / MG / SG / SMG
- Sidearm: Pistol
- Melee: Melee
- Backpack 1 / 2: any enabled supported category

Future: weapon imagery, icons, unlock state and progression.

### Tools
Tools hub, inspired by Open1320's Modding page:
- Weapon Editor
- Mod Library
- Content Tools / LithTech SDK
- future Round / Powerup Tuner
- future infected / character editors
- future REZ packaging and mod management

Weapon authoring is not a top-level player tab.

### Settings

#### Display
- resolution
- windowed / fullscreen
- gamma
- future VSync after engine CVar verification
- future FPS limit after engine CVar verification

#### Input
- horizontal mouse sensitivity
- vertical mouse sensitivity
- future invert/raw-input options after engine verification

#### Audio
- current game/master volume
- future categories only if the runtime supports them

#### Developer
- future debug menu
- future console / FPS display
- only after the real runtime variables are confirmed

## Execution phases

### Phase 1 — shell + functional cleanup
- top navigation
- compact Home dashboard
- grouped cards and stronger FIRETEAM visual identity
- Tools hub
- move Weapon Editor under Tools
- compact Weapon Editor layout
- separate X/Y sensitivity
- fix loadout weapon enumeration/dropdowns
- preserve current stable code-built WinUI startup path

### Phase 1.1 — weapon import workflow
- full local extraction of Guns.zip / GunsHH.zip into ignored assets-local storage
- one-click staging command for the complete imported asset tree into BUILT\rez
- Weapon Editor Enabled/Disabled control
- import decrypted Combat Arms WEAPONS.txt
- import/stage Guns.zip and auto-detect adjacent GunsHH.zip assets
- generate disabled catalog entries for testing
- one-off asset/config scanner before gameplay testing
- scanner command validates imported asset references in local source + BUILT runtime

### Phase 1.2 — loadout armory
- CA Guntype mapping drives launcher inventory categories
- three saved loadout presets in config/loadouts.cfg
- Combat Arms-inspired category browser + stat inspector
- player equipment slots remain FIRETEAM-specific and data-driven

### Phase 2 — player-facing polish
- weapon cards / icons
- richer loadout summaries
- news data source
- selected-nav styling / hover menus
- server browser shell
- map selection when more official maps exist

### Phase 3 — content-tool windows
- open Weapon Editor as its own tool window
- dedicated round/powerup/infected/character tools
- mod enable/disable/version/conflict UI
- DEdit / ModelEdit / FXed / RenderStyleEditor launchers

### Phase 4 — progression / community
- unlocks
- account/profile presentation if needed
- community content discovery
- progression stats

## Phase 1 acceptance checks

- launcher starts with no startup.log exception
- title says FIRETEAM only
- navigation is across the top
- Home keeps the primary LAUNCH GAME action visible in the Ready Room header without scrolling
- News/Updates appears in a right-side column
- Loadout dropdowns enumerate current weapon definitions
- Weapon Editor is only reached through Tools / Loadout
- Settings exposes separate horizontal and vertical sensitivity
- saving settings preserves distinct X/Y values in fireteam-settings.cfg
- no regression to Play, config paths, Mods or Content Tools
