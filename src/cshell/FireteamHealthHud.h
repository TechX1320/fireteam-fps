#ifndef __FIRETEAM_HEALTH_HUD_H__
#define __FIRETEAM_HEALTH_HUD_H__

#include <ltbasedefs.h>

void FT_SetHealth(uint8 nHealth, uint8 nMaxHealth);
void FT_RenderPoisonOverlay(HLOCALOBJ hPlayer);
void FT_RenderHealthHud();

#endif