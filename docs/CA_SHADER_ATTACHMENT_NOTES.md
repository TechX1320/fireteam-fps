# Combat Arms shader and attachment notes

Source inspected: the supplied `SHADERS.zip` and `ATTACHMENTS.zip`.

## Shader archive

`SHADERS.zip` contains 18 files.

Fullscreen FX:
- `FULLSCREEN/BLACKANDWHITE.FX`
- `FULLSCREEN/BLOOM.FX`
- `FULLSCREEN/GBLUR.FX`
- `FULLSCREEN/JITTER.FX`
- `FULLSCREEN/NIGHTVISION.FX`
- `FULLSCREEN/SATURATE.FX`

Supporting image assets include focus masks, heat/night noise, lookup textures and scope images. `SPRITE/REFRACT.FX` is also present.

The bloom shader is a real multipass PS/VS 2.0 effect with downsample, horizontal/vertical blur and final composition. The saturation shader is a fullscreen desaturation pass. Night vision consumes the current render target plus noise/lookup textures.

### What this means for FIRETEAM

The shader pack is relevant, but it is not a drop-in lighting fix. Jupiter has effect-shader and render-target support, while the current FIRETEAM client shell does not yet register or drive these CA fullscreen effects.

The build now stages the archive to `rez/Shaders` so the paths are ready. A later graphics pass should:
1. register a selected effect shader with the renderer,
2. render/copy the scene into the expected render target(s),
3. bind CA parameter names such as `vViewportSize` and scene textures,
4. draw the fullscreen pass after world rendering but before HUD,
5. expose effects as optional launcher/video settings while parity is tested.

Bloom/desaturation can materially change the Combat Arms look. They should not be used to hide unresolved map lighting, gamma, render-style or material differences.

## Attachment archive

`ATTACHMENTS.zip` contains 585 files:
- 227 `ATTACH_M` model files (LTB)
- 358 `ATTACH_T` texture/sprite files (DTX/SPR)

The names show that this archive is primarily character equipment/cosmetics: backpacks, masks, glasses, helmets/headgear and related variants. Examples include four-slot backpacks, SWAT/tactical packs, rifle-carrier packs, anti-flash glasses and masks.

### Useful future systems

This archive is a strong source for:
- visual backpack equipment,
- armor/backpack tiers shown on the player model,
- cosmetic masks/glasses/headgear,
- pickup/reward visuals.

It is not the main source for weapon scopes, extended magazines or suppressors. Those will need weapon-specific model/attachment data from the gun archives or another CA attachment data source.

Recommended future implementation is data-driven: attachment id, model, texture(s), target player socket/node, gameplay modifiers, and optional inventory slot. Do not hard-code hundreds of CA filenames in C++.
