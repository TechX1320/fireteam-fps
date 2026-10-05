#ifndef __FIRETEAM_WEAPON_HUD_H__
#define __FIRETEAM_WEAPON_HUD_H__

#include <ltbasedefs.h>

void FT_WeaponHudInit();
void FT_WeaponHudTerm();
void FT_SetPrimaryAmmo(uint16 nClip, uint16 nReserve);
bool FT_ConsumePrimaryAmmoLocal();
void FT_RenderWeaponHud(uint8 nWeaponSlot, bool bFirstPerson);

#endif