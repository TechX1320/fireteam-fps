# Xbox-style controller layout

FIRETEAM now polls XInput directly instead of depending on the 2002-era
LithTech joystick binding path.

The target layout intentionally follows the reference Xbox layout supplied
during development.

## Live controls

| Control | FIRETEAM action |
| --- | --- |
| Left stick | Analog move |
| Right stick | Look |
| LT | Hold zoom for scoped weapons |
| RT | Fire weapon |
| L3 | Sprint |
| A | Jump |
| B | Crouch |
| X | Reload |
| Y | Switch to next weapon |
| R3 | Melee slot + attack |
| View / Back | Hold scoreboard |
| Menu / Start | Tap game/settings menu |
| Menu / Start | Hold about 0.55 sec for text chat |
| D-pad in settings | Navigate/change |
| A in settings | Select |
| B in settings | Back/resume |

R3 currently equips loadout slot 3 and immediately attacks. It is not yet a
true temporary quick-melee that automatically restores the prior weapon.

## Reserved to match the target layout

These controls are deliberately reserved instead of being assigned to unrelated
actions:

| Control | Planned action |
| --- | --- |
| LB | Throw grenade |
| RB | Use equipment |
| D-pad Up | Mark / helmet light |
| D-pad Left | Grenade switcher |
| D-pad Down | AI scan / future utility |
| D-pad Right | Drop weapon |
| Hold X | Interact |
| Hold Y | Switch equipment |

Those bindings become live as the corresponding systems are implemented.

## Implementation

- Client-side input module: `src/cshell/FireteamController.cpp`
- API/state: `src/cshell/FireteamController.h`
- Windows API: XInput
- Link target: `xinput9_1_0`
- Left and right thumbstick deadzones use Microsoft's XInput defaults.
- Left-stick magnitude is preserved, so controller movement is genuinely
  analog rather than converted to digital WASD.
