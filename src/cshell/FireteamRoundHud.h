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
void FT_RenderRoundHud();

#endif
