# Combat Arms LTB animation notes

These notes are derived directly from the supplied Combat Arms LTB files. They
exist so animation names do not need to be guessed by cycling models in-game.

## Normal male / infected animation child

File:

`CHARS_M_BODY/ST_M_CHILD.LTB`

Confirmed animation-name strings in its animation table include:

### Base / idle

- `baseAnim`
- `select`
- `C_info`
- `C_idle_0`
- `K_idle_0`
- `K_info`

### Lower-body movement families

- `LCFR`
- `LCFL`
- `LCBR`
- `LCBL`
- `LJJR`
- `LJJL`
- `LJBR`
- `LJBL`
- `LRFR`
- `LRFL`
- `LRBR`
- `LRBL`
- `LWFR`
- `LWFL`
- `LWBR`
- `LWBL`
- `LRRR`
- `LRRL`
- `LRRF`
- `LRRJF`
- `LRRJR`
- `LRRJL`

The exact semantic mapping of every directional suffix still needs to be
verified against original Combat Arms behavior before FIRETEAM treats one as
the canonical infected run cycle.

### Death / hit family

- `D_DU`
- `D_DA`
- `VDIE`
- `DC_1`
- `DCH_1`
- `DH_1`
- `DT_1`
- `DCB_1`
- `DCBH`
- `DCBH_1`
- `DHB_1`
- `DTB_1`
- `DRSH`
- `DLSH`
- `D_HIT`
- `D_HITT`
- `D_BO_F`
- `D_BO_B`
- `D_BO_L`
- `D_BO_R`

This is enough to build deterministic death-animation tests without converting
LTB to LTA. LTA conversion is still useful for dimensions, nodes, sockets and
keyframe inspection.

## Weapon animation companions

Confirmed directly from the supplied `Guns.zip`:

### AK-47 — `AK47_ANIBASE.LTB`

- `fire_0`
- `fire_1`
- `idle_0`
- `idle_1`
- `reload`
- `select`

### Beretta M92FS — `BERETTA_ANIBASE.LTB`

- `fire_0`
- `fire_1`
- `idle_0`
- `idle_1`
- `reload`
- `select`

### Colt 1911A1 MEU — `COLT_MEU_ANIBASE.LTB`

- `fire`
- `fire1`
- `idle_0`
- `idle_1`
- `reload`
- `select`

### L96A1 — `L96A1_ANIBASE.LTB`

- `fire_0`
- `fire_1`
- `idle_0`
- `idle_1`
- `reload`
- `select`

The generic FIRETEAM weapon animation code intentionally falls back from
`fire_0` to `fire`, which covers the Colt companion.

## Modular infected head assets

The supplied character archive contains all of the following matching virus
assets:

- `CHARS_M_FACE/CM_FC_NM_VIRUS_HM.LTB`
- `CHARS_T_FACE/CM_FC_NM_VIRUS_HM.DTX`
- `CHARS_M_HEAD/CM_HLMT_NM_VIRUS_HM.LTB`
- `CHARS_T_HEAD/CM_HLMT_NM_VIRUS_HM.DTX`

Both LTBs contain the expected `Head` / facial skeleton hierarchy. FIRETEAM
now stages and tests both modular children because body + face alone was still
visibly headless. This is a compatibility test, not yet a claim that the exact
original CA attachment recipe has been fully recovered.
