# Local game assets

This folder is intentionally ignored by Git because the test assets are extracted from commercial game data.

## Cabin Fever

Place these files here:

- `CABINFEVER.DAT`
- `TEXTURES.zip`
- `FX.zip`
- `RS.zip`

## Bowie knife

Also place the original archives here:

- `Guns.zip`
- `GunsHH.zip`

The build does **not** unpack the entire weapon archives. It stages only the first Bowie test assets:

- `GUNS_M_PV_MELEE/CM_HND_NM_DF_BOWIEKNIFE_CH.LTB`
- `GUNS_M_PV_MELEE/ANI_G_BOWIEKNIFE_CH.LTB`
- `GUNS_T_PV_MELEE/PV_ML_DF_BOWIEKNIFE_BC.DTX`
- `GUNS_SND_MELEE/BOWIE_KNIFE/FIRE.WAV`
- `GUNS_SND_MELEE/BOWIE_KNIFE/SELECT.WAV`
- `GUNS_M_HH/HH_ML_DF_BOWIEKNIFE_CH.LTB`
- `GUNS_T_HH/HH_ML_DF_BOWIEKNIFE_BC.DTX`

The first-person model references `ANI_G_BOWIEKNIFE_CH.LTB` for animations. Confirmed animation names include `select`, `idle_0`, `idle_1`, `fire_0`, `fire_1`, `alt_fire_0`, and `alt_fire_1`.

After adding or changing local assets, just run `build.cmd`. Missing optional archives do not stop setup so map resources can be filled in progressively.
