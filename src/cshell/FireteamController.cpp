#include "FireteamController.h"

#include <windows.h>
#include <Xinput.h>
#include <string.h>

static DWORD s_nControllerIndex = 0xFFFFFFFF;
static uint16 s_nPreviousButtons = 0;
static bool s_bPreviousLeftTrigger = false;
static bool s_bPreviousRightTrigger = false;
static bool s_bWasConnected = false;

static float FT_NormalizeStick(SHORT nValue, SHORT nDeadZone)
{
    const int nRaw = (int)nValue;
    const int nAbs = nRaw < 0 ? -nRaw : nRaw;

    if(nAbs <= (int)nDeadZone)
    {
        return 0.0f;
    }

    float fValue =
        (float)(nAbs - nDeadZone) /
        (float)(32767 - nDeadZone);

    if(fValue > 1.0f)
        fValue = 1.0f;

    return nRaw < 0 ? -fValue : fValue;
}

static float FT_NormalizeTrigger(BYTE nValue)
{
    if(nValue <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
    {
        return 0.0f;
    }

    return
        (float)(nValue - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
        (float)(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}

void FT_ControllerReset()
{
    s_nControllerIndex = 0xFFFFFFFF;
    s_nPreviousButtons = 0;
    s_bPreviousLeftTrigger = false;
    s_bPreviousRightTrigger = false;
    s_bWasConnected = false;
}

bool FT_ControllerPoll(FTControllerState &state)
{
    memset(&state, 0, sizeof(state));

    XINPUT_STATE xstate;
    memset(&xstate, 0, sizeof(xstate));

    DWORD nResult = ERROR_DEVICE_NOT_CONNECTED;

    if(s_nControllerIndex < 4)
    {
        nResult = XInputGetState(s_nControllerIndex, &xstate);
    }

    if(nResult != ERROR_SUCCESS)
    {
        s_nControllerIndex = 0xFFFFFFFF;

        for(DWORD i = 0; i < 4; ++i)
        {
            memset(&xstate, 0, sizeof(xstate));

            if(XInputGetState(i, &xstate) == ERROR_SUCCESS)
            {
                s_nControllerIndex = i;
                nResult = ERROR_SUCCESS;
                break;
            }
        }
    }

    if(nResult != ERROR_SUCCESS)
    {
        state.nReleased =
            s_bWasConnected ? s_nPreviousButtons : 0;
        state.bLeftTriggerReleased =
            s_bWasConnected && s_bPreviousLeftTrigger;
        state.bRightTriggerReleased =
            s_bWasConnected && s_bPreviousRightTrigger;

        s_nPreviousButtons = 0;
        s_bPreviousLeftTrigger = false;
        s_bPreviousRightTrigger = false;
        s_bWasConnected = false;
        return false;
    }

    state.bConnected = true;

    state.fMoveX =
        FT_NormalizeStick(
            xstate.Gamepad.sThumbLX,
            XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    state.fMoveY =
        FT_NormalizeStick(
            xstate.Gamepad.sThumbLY,
            XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    state.fLookX =
        FT_NormalizeStick(
            xstate.Gamepad.sThumbRX,
            XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    state.fLookY =
        FT_NormalizeStick(
            xstate.Gamepad.sThumbRY,
            XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

    state.fLeftTrigger =
        FT_NormalizeTrigger(xstate.Gamepad.bLeftTrigger);
    state.fRightTrigger =
        FT_NormalizeTrigger(xstate.Gamepad.bRightTrigger);

    state.bLeftTriggerDown = state.fLeftTrigger > 0.0f;
    state.bRightTriggerDown = state.fRightTrigger > 0.0f;
    state.nButtons = (uint16)xstate.Gamepad.wButtons;

    if(s_bWasConnected)
    {
        state.nPressed =
            (uint16)(state.nButtons & ~s_nPreviousButtons);
        state.nReleased =
            (uint16)(s_nPreviousButtons & ~state.nButtons);

        state.bLeftTriggerPressed =
            state.bLeftTriggerDown && !s_bPreviousLeftTrigger;
        state.bLeftTriggerReleased =
            !state.bLeftTriggerDown && s_bPreviousLeftTrigger;
        state.bRightTriggerPressed =
            state.bRightTriggerDown && !s_bPreviousRightTrigger;
        state.bRightTriggerReleased =
            !state.bRightTriggerDown && s_bPreviousRightTrigger;
    }

    s_nPreviousButtons = state.nButtons;
    s_bPreviousLeftTrigger = state.bLeftTriggerDown;
    s_bPreviousRightTrigger = state.bRightTriggerDown;
    s_bWasConnected = true;

    return true;
}
