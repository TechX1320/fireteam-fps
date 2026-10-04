# Local game assets

This folder is intentionally ignored by Git because the test assets are extracted from commercial game data.

For the Cabin Fever bring-up test, place these files here:

- CABINFEVER.DAT
- TEXTURES.zip
- FX.zip
- RS.zip

Then rerun setup-local.cmd followed by build.cmd.

The setup script stages them locally as:

- CABINFEVER.DAT -> rez\Worlds\CABINFEVER.DAT
- TEXTURES.zip -> rez\Textures\
- FX.zip -> rez\FX\
- RS.zip -> rez\RenderStyles\

Missing optional archives do not stop setup, so we can progressively add resources as the map reports them.
