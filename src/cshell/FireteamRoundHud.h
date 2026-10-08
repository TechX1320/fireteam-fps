#ifndef __FIRETEAM_ROUND_HUD_H__
#define __FIRETEAM_ROUND_HUD_H__

#include <ltbasedefs.h>

class ILTMessage_Read;

void FT_RoundHudInit();
void FT_RoundHudTerm();
void FT_RoundHudHandleMessage(ILTMessage_Read *pMessage);
void FT_RoundHudShowAnnouncement(
    const char *pText,
    float fSeconds);
void FT_RoundHudSetTimedPowerups(
    float fBottomlessSeconds,
    float fOneHitSeconds,
    float fGodSeconds,
    float fWallhackSeconds);
void FT_RoundHudSetLives(
    uint8 nLives,
    uint8 nMaxLives);
bool FT_RoundHudIsPlayerEliminated();
bool FT_RoundHudIsGameOver();
void FT_RoundHudSetSpectator(
    bool bSpectating,
    bool bQaMode);
void FT_RenderRoundHud();

#endif
