#ifndef __FIRETEAM_HEALTH_HUD_H__
#define __FIRETEAM_HEALTH_HUD_H__
#include <ltbasedefs.h>
#include <ltvector.h>
void FT_HealthHudInit();
void FT_HealthHudTerm();
void FT_HealthHudReset();
void FT_SetHealth(uint8 nHealth, uint8 nMaxHealth);
void FT_HealthHudDamage(const LTVector &vSource, uint8 nDamage);
void FT_RenderPoisonOverlay(HLOCALOBJ hPlayer);
void FT_RenderHealthHud(HOBJECT hCamera);
#endif
