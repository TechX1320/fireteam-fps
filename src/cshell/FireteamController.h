#ifndef __FIRETEAM_CONTROLLER_H__
#define __FIRETEAM_CONTROLLER_H__

#include <ltbasedefs.h>

enum EFTControllerButton
{
    FT_PAD_DPAD_UP    = 0x0001,
    FT_PAD_DPAD_DOWN  = 0x0002,
    FT_PAD_DPAD_LEFT  = 0x0004,
    FT_PAD_DPAD_RIGHT = 0x0008,
    FT_PAD_MENU       = 0x0010,
    FT_PAD_VIEW       = 0x0020,
    FT_PAD_LTHUMB     = 0x0040,
    FT_PAD_RTHUMB     = 0x0080,
    FT_PAD_LB         = 0x0100,
    FT_PAD_RB         = 0x0200,
    FT_PAD_A          = 0x1000,
    FT_PAD_B          = 0x2000,
    FT_PAD_X          = 0x4000,
    FT_PAD_Y          = 0x8000
};

struct FTControllerState
{
    bool bConnected;
    float fMoveX;
    float fMoveY;
    float fLookX;
    float fLookY;
    float fLeftTrigger;
    float fRightTrigger;
    uint16 nButtons;
    uint16 nPressed;
    uint16 nReleased;
    bool bLeftTriggerDown;
    bool bRightTriggerDown;
    bool bLeftTriggerPressed;
    bool bLeftTriggerReleased;
    bool bRightTriggerPressed;
    bool bRightTriggerReleased;
};

bool FT_ControllerPoll(FTControllerState &state);
void FT_ControllerReset();

#endif
