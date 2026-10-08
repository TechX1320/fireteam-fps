# FIRETEAM fast build / HUD gameplay QA (October 2026)

## What changed

- `scripts/refresh-engine-import.ps1` no longer reads and hashes the entire
  `EngineMissing*.zip` when the archive's path/size/UTC modification time
  matches the last successfully verified SHA256. It still hashes when changed.
- `scripts/stage-local-assets.ps1` avoids reopening large character archives
  for **previously staged unchanged ZIP entries** (including
  `ANI_VI_TANKER_SH.LTB`).
- Printable-string animation candidate reports for `ST_M_CHILD`,
  `ANI_VI_ASSASSIN_CH`, and `ANI_VI_TANKER_SH` are regenerated only when
  the relevant LTB metadata or report version changes, or a report is missing.
- No game assets were committed or changed in the GitHub repository.
- Normal build and launcher targets remain unchanged.

The FIRST build after pulling will initialize missing cache stamps. The
SECOND unchanged build is the real speed comparison; look for `[FAST]`
messages. ZIP/LTB file identity, size and UTC modification time invalidate the
cache automatically. If bytes were replaced without changing any of those
attributes (rare), the build cannot detect it by metadata alone.

For a one-run complete integrity/re-audit pass from Command Prompt:

```bat
set FT_FORCE_ASSET_AUDIT=1
build.cmd
set FT_FORCE_ASSET_AUDIT=
```

This bypasses the new metadata fast paths; it does **not** delete existing
files or forcibly overwrite hand-modified staged assets. Other operations
(including `xcopy`, the weapon import sync and DAT texture audits) may still
take time. Measure those separately if a subsequent build is still slow.

## Playtest

1. Pull and `build.cmd`. Report the first MSVC error if any.
2. Start Cabin Fever, confirm “ROUND N BEGIN” text disappears after about
   **3 seconds** and that the separate round-start artwork still fades.
3. Collect a Mutation Box bonus. Its temporary announcement should also
   clear after the configured duration, including across a round intermission.
   The active buff timer should remain paused during intermission.
4. Empty the magazine while holding the fire button through an automatic
   reload. One reload animation/sound should play, then shooting resumes
   when authoritative ammo is received. The new two-second acknowledgment
   window only affects the local trigger/reload presentation.
5. Verify manual reload, switching weapons during a reload, zero-reserve ammo
   and Bottomless Mag still work.
6. GAME OVER persists intentionally until another round/world replaces it.

The TAB player list in `src/cshell/statsgui.cpp` still inherits a 2003
layout that positions column headings using spaces in a proportional font.
A redesigned multiplayer scoreboard with real score/down/revive/ping fields
will need a separate UI and server-protocol pass.
