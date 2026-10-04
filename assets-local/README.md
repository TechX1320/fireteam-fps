# Local game assets

This folder is intentionally ignored by Git because the test assets are extracted from commercial game data.

## Cabin Fever

Place these files here:

- `CABINFEVER.DAT`
- `TEXTURES.zip`
- `FX.zip`
- `RS.zip`

The staging script now reads the texture paths embedded in `CABINFEVER.DAT`. When the DAT expects a flattened path such as `Textures\Objects\roof01.dtx` but the extracted archive stores that file deeper (for example `Textures\OBJECTS\CLOTHINGS\ROOF01.DTX`), it creates a local alias automatically. Ambiguous non-identical matches are reported instead of guessed.

## Combat Arms ClientFX

For the original Combat Arms FxED database, place these files here:

- `CLIENTFX.zip` containing `CLIENTFX.FXF` and `CLIENTFX.FCF`
- `ClientFx.fxd`

These locally replace the small SealHunter ClientFX database at build staging time. Cabin Fever references effects such as `Foggy_Vio`, which exist in the Combat Arms ClientFX data.

## Bowie knife

Place the original archives here:

- `Guns.zip`
- `GunsHH.zip`
- `BOWIE_KNIFE.zip` (optional dedicated FIRE/SELECT sound pack; preferred when present)
- `CharModels-Textur.zip` (currently supplies the test player hand skin and staged Specialist character pieces)

The build does **not** unpack the entire weapon or character archives. It stages only the currently used test assets.

Current Fireteam layout:

```text
Weapons/
  melee_m_pv/
  melee_m_hh/
  melee_t/
  melee_snd/BOWIE_KNIFE/

Characters/
  male/body/
  male/face/
  male/hands/
  male/textures/
```

The first-person Bowie model references `ANI_G_BOWIEKNIFE_CH.LTB` for animations. Confirmed animation names include `select`, `idle_0`, `idle_1`, `fire_0`, `fire_1`, `alt_fire_0`, and `alt_fire_1`.

After adding or changing local assets, just run `build.cmd`. Missing optional archives do not stop setup so map resources can be filled in progressively.
