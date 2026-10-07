#ifndef __FIRETEAM_WEAPON_HUD_H__
#define __FIRETEAM_WEAPON_HUD_H__

#include <ltbasedefs.h>

void FT_WeaponHudInit();
void FT_WeaponHudTerm();
void FT_SetPrimaryAmmo(uint16 nClip, uint16 nReserve);
void FT_WeaponHudOnShot(uint8 nWeaponSlot);
void FT_WeaponHudSetWeaponDefinition(
    uint8 nWeaponSlot,
    const FTWeaponDef *pDef);
void FT_WeaponHudSetQaProgress(
    uint32 nCurrent,
    uint32 nTotal);
void FT_RenderWeaponHud(
    uint8 nWeaponSlot,
    bool bFirstPerson,
    bool bShowCrosshair,
    bool bShowAmmo,
    bool bMoving,
    bool bScoped);

#endif