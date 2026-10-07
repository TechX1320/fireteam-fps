# Combat Arms sound notes

Source inspected: the supplied all-sounds `SND(2).zip` (259 archive entries).

## Cabin Fever map audio

Confirmed original Cabin Fever files:

- `CABINFEVER/RAIN_INDOOR.WAV`
- `CABINFEVER/RAIN_OUTDOOR.WAV`
- `CABINFEVER/SCREAM1.WAV` through `SCREAM4.WAV`
- `CABINFEVER/SECTION1.WAV`
- `CABINFEVER/SECTION2.WAV`
- `CABINFEVER/LIGHTOFF.WAV`
- `CABINFEVER/ENDING.WAV`

The map also contains many authored `SoundFX` objects. FIRETEAM has a
compatible `SoundFX` class, so staged SND assets can be consumed by those
objects instead of forcing every ambience layer into C++.

`SECTION1.WAV` and `SECTION2.WAV` are currently borrowed as the audible
round-start / round-clear stings.

## Cabin Fever infected / NPC voice banks

The archive contains four distinct banks:

- `COOPMODE/NPC_VOICE/CABINFEVER/NORMAL`
- `COOPMODE/NPC_VOICE/CABINFEVER/LIGHT`
- `COOPMODE/NPC_VOICE/CABINFEVER/HEAVY`
- `COOPMODE/NPC_VOICE/CABINFEVER/EXPLODE`

`NORMAL` contains `SEEENEMY.WAV`, `ATTACK1.WAV` through `ATTACK3.WAV`
and `DEATH.WAV`. FIRETEAM's current normal infected is wired to this exact
bank through `config/infected.cfg`.

The LIGHT bank also has a death sound. HEAVY and EXPLODE have separate alert
and attack vocals and are reserved for later data-driven infected variants.

The EXPLODE folder additionally contains a large human/NPC voice set, including
contact, cover, hit and kill lines. Those are useful source material for future
NPC/special-infected work, but are not treated as the game announcer.

## Useful borrowed atmosphere

The archive also provides reusable environment layers such as:

- `UNIVERSAL/AMBIENT/ROOM_*.WAV`
- `UNIVERSAL/AMBIENT/HALL_*.WAV`
- `UNIVERSAL/AMBIENT/CAVE_AMB.WAV`
- `RATTLE_SNAKE/OWLING*.WAV`
- `ROADKILL/CROW_01.WAV`
- `DARKFOREST/DRIPPING_WATER.WAV`
- `GRAVEDIGGER/WATER_DRIPPING_*.WAV`
- `HALLOWRAVINE/CAVE_AMBIANT.WAV`
- `HALLOWRAVINE/MOUNT_WIND.WAV`

These are candidates for future authored SoundFX placement rather than
automatic global loops.

## Hit-announcement audio

This archive does not contain a literally named HEADSHOT, NUTSHOT, FIRSTKILL,
DOUBLEKILL, MULTIKILL or announcer voice bank.

FIRETEAM therefore uses the real CA HUD art for those events and currently
borrows short Training target cues as low-volume local hit confirmation:

- Headshot: `Snd/TRAINING/TARGET_DOWN1.WAV`
- Nut Shot: `Snd/TRAINING/TARGET_UP1.WAV`

If the authentic Combat Arms announcer VO archive is found later, only the
client cue paths need to change.
