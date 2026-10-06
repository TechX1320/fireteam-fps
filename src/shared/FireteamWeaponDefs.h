#ifndef __FIRETEAM_WEAPON_DEFS_H__
#define __FIRETEAM_WEAPON_DEFS_H__

#include <ltbasedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum FTWeaponType
{
    FT_WEAPON_NONE = 0,
    FT_WEAPON_HITSCAN,
    FT_WEAPON_MELEE,
    FT_WEAPON_GRENADE,
    FT_WEAPON_ROCKET
};

struct FTWeaponDef
{
    uint8 nSlot;
    char  sId[32];
    char  sName[64];
    FTWeaponType eType;

    uint16 nClipSize;
    uint16 nStartReserve;
    uint8  nDamage;

    float fFireInterval;
    float fRange;
    float fEffectRange0;
    float fEffectRange1;
    float fEffectRange2;
    float fDamageMult0;
    float fDamageMult1;
    float fDamageMult2;
    float fReloadSeconds;
    bool  bAutomatic;
    bool  bAutoReload;
    bool  bShowCrosshair;

    // Optional first-person optic behavior. A zoom FOV <= 0 means no optic.
    float fZoomFovDegrees;
    bool  bZoomHideWeapon;

    // HUD presentation only. Ballistic spread remains server-authoritative
    // and can be added as separate gameplay fields later.
    float fCrosshairBaseGap;
    float fCrosshairShotKick;
    float fCrosshairMoveKick;
    float fCrosshairMaxGap;
    float fCrosshairRecover;

    float fProjectileSpeed;
    float fProjectileScale;
    float fSplashRadius;
    float fFuseSeconds;

    char sProjectileModel[128];
    char sProjectileTexture[128];

    float fViewX;
    float fViewY;
    float fViewZ;

    char sPVModel[128];
    char sPVAnim[128];
    char sPVTexture[128];
    char sHHModel[128];
    char sHHTexture[128];
    char sSoundDir[128];
};

inline void FT_CopyWeaponString(char *pDest, uint32 nDestLen, const char *pSrc)
{
    if(!pDest || nDestLen == 0)
    {
        return;
    }

    if(!pSrc)
    {
        pDest[0] = '\0';
        return;
    }

    strncpy(pDest, pSrc, nDestLen - 1);
    pDest[nDestLen - 1] = '\0';
}

inline char* FT_TrimWeaponLine(char *pText)
{
    if(!pText)
    {
        return pText;
    }

    while(*pText && isspace((unsigned char)*pText))
    {
        ++pText;
    }

    char *pEnd = pText + strlen(pText);
    while(pEnd > pText && isspace((unsigned char)pEnd[-1]))
    {
        --pEnd;
    }

    *pEnd = '\0';
    return pText;
}

inline FTWeaponType FT_ParseWeaponType(const char *pValue)
{
    if(!pValue) return FT_WEAPON_NONE;
    if(_stricmp(pValue, "hitscan") == 0) return FT_WEAPON_HITSCAN;
    if(_stricmp(pValue, "melee") == 0) return FT_WEAPON_MELEE;
    if(_stricmp(pValue, "grenade") == 0) return FT_WEAPON_GRENADE;
    if(_stricmp(pValue, "rocket") == 0) return FT_WEAPON_ROCKET;
    return FT_WEAPON_NONE;
}

inline void FT_InitWeaponDefaults(FTWeaponDef aDefs[6])
{
    // Content lives in config/weapons.cfg. Keep only format/engine semantics
    // here so adding or replacing weapons never requires recompiling C++.
    memset(aDefs, 0, sizeof(FTWeaponDef) * 6);

    for(uint8 nSlot = 1; nSlot <= 5; ++nSlot)
    {
        aDefs[nSlot].nSlot = nSlot;
        aDefs[nSlot].fProjectileScale = 1.0f;
    }
}

inline bool FT_LoadWeaponDefs(const char *pFilename, FTWeaponDef aDefs[6])
{
    FT_InitWeaponDefaults(aDefs);

    FILE *pFile = fopen(pFilename, "rt");
    if(!pFile)
    {
        return false;
    }

    char sLine[512];
    int nCurrentSlot = 0;

    while(fgets(sLine, sizeof(sLine), pFile))
    {
        char *pLine = FT_TrimWeaponLine(sLine);
        if(!pLine[0] || pLine[0] == '#' || pLine[0] == ';')
        {
            continue;
        }

        if(pLine[0] == '[')
        {
            int nSlot = 0;
            if(sscanf(pLine, "[weapon%d]", &nSlot) == 1 && nSlot >= 1 && nSlot <= 5)
            {
                nCurrentSlot = nSlot;
            }
            else
            {
                nCurrentSlot = 0;
            }
            continue;
        }

        if(nCurrentSlot < 1 || nCurrentSlot > 5)
        {
            continue;
        }

        char *pEquals = strchr(pLine, '=');
        if(!pEquals)
        {
            continue;
        }

        *pEquals = '\0';
        char *pKey = FT_TrimWeaponLine(pLine);
        char *pValue = FT_TrimWeaponLine(pEquals + 1);
        FTWeaponDef &def = aDefs[nCurrentSlot];

        if(_stricmp(pKey, "id") == 0) FT_CopyWeaponString(def.sId, sizeof(def.sId), pValue);
        else if(_stricmp(pKey, "name") == 0) FT_CopyWeaponString(def.sName, sizeof(def.sName), pValue);
        else if(_stricmp(pKey, "type") == 0) def.eType = FT_ParseWeaponType(pValue);
        else if(_stricmp(pKey, "clip") == 0) def.nClipSize = (uint16)atoi(pValue);
        else if(_stricmp(pKey, "reserve") == 0) def.nStartReserve = (uint16)atoi(pValue);
        else if(_stricmp(pKey, "damage") == 0) def.nDamage = (uint8)atoi(pValue);
        else if(_stricmp(pKey, "fire_interval") == 0) def.fFireInterval = (float)atof(pValue);
        else if(_stricmp(pKey, "range") == 0) def.fRange = (float)atof(pValue);
        else if(_stricmp(pKey, "effect_range0") == 0) def.fEffectRange0 = (float)atof(pValue);
        else if(_stricmp(pKey, "effect_range1") == 0) def.fEffectRange1 = (float)atof(pValue);
        else if(_stricmp(pKey, "effect_range2") == 0) def.fEffectRange2 = (float)atof(pValue);
        else if(_stricmp(pKey, "damage_mult0") == 0) def.fDamageMult0 = (float)atof(pValue);
        else if(_stricmp(pKey, "damage_mult1") == 0) def.fDamageMult1 = (float)atof(pValue);
        else if(_stricmp(pKey, "damage_mult2") == 0) def.fDamageMult2 = (float)atof(pValue);
        else if(_stricmp(pKey, "reload") == 0) def.fReloadSeconds = (float)atof(pValue);
        else if(_stricmp(pKey, "automatic") == 0) def.bAutomatic = atoi(pValue) != 0;
        else if(_stricmp(pKey, "auto_reload") == 0) def.bAutoReload = atoi(pValue) != 0;
        else if(_stricmp(pKey, "show_crosshair") == 0) def.bShowCrosshair = atoi(pValue) != 0;
        else if(_stricmp(pKey, "zoom_fov") == 0) def.fZoomFovDegrees = (float)atof(pValue);
        else if(_stricmp(pKey, "zoom_hide_weapon") == 0) def.bZoomHideWeapon = atoi(pValue) != 0;
        else if(_stricmp(pKey, "crosshair_base_gap") == 0) def.fCrosshairBaseGap = (float)atof(pValue);
        else if(_stricmp(pKey, "crosshair_shot_kick") == 0) def.fCrosshairShotKick = (float)atof(pValue);
        else if(_stricmp(pKey, "crosshair_move_kick") == 0) def.fCrosshairMoveKick = (float)atof(pValue);
        else if(_stricmp(pKey, "crosshair_max_gap") == 0) def.fCrosshairMaxGap = (float)atof(pValue);
        else if(_stricmp(pKey, "crosshair_recover") == 0) def.fCrosshairRecover = (float)atof(pValue);
        else if(_stricmp(pKey, "projectile_speed") == 0) def.fProjectileSpeed = (float)atof(pValue);
        else if(_stricmp(pKey, "projectile_scale") == 0) def.fProjectileScale = (float)atof(pValue);
        else if(_stricmp(pKey, "projectile_model") == 0) FT_CopyWeaponString(def.sProjectileModel, sizeof(def.sProjectileModel), pValue);
        else if(_stricmp(pKey, "projectile_texture") == 0) FT_CopyWeaponString(def.sProjectileTexture, sizeof(def.sProjectileTexture), pValue);
        else if(_stricmp(pKey, "splash_radius") == 0) def.fSplashRadius = (float)atof(pValue);
        else if(_stricmp(pKey, "fuse") == 0) def.fFuseSeconds = (float)atof(pValue);
        else if(_stricmp(pKey, "view_x") == 0) def.fViewX = (float)atof(pValue);
        else if(_stricmp(pKey, "view_y") == 0) def.fViewY = (float)atof(pValue);
        else if(_stricmp(pKey, "view_z") == 0) def.fViewZ = (float)atof(pValue);
        else if(_stricmp(pKey, "pv_model") == 0) FT_CopyWeaponString(def.sPVModel, sizeof(def.sPVModel), pValue);
        else if(_stricmp(pKey, "pv_anim") == 0) FT_CopyWeaponString(def.sPVAnim, sizeof(def.sPVAnim), pValue);
        else if(_stricmp(pKey, "pv_texture") == 0) FT_CopyWeaponString(def.sPVTexture, sizeof(def.sPVTexture), pValue);
        else if(_stricmp(pKey, "hh_model") == 0) FT_CopyWeaponString(def.sHHModel, sizeof(def.sHHModel), pValue);
        else if(_stricmp(pKey, "hh_texture") == 0) FT_CopyWeaponString(def.sHHTexture, sizeof(def.sHHTexture), pValue);
        else if(_stricmp(pKey, "sound_dir") == 0) FT_CopyWeaponString(def.sSoundDir, sizeof(def.sSoundDir), pValue);
    }

    fclose(pFile);
    return true;
}

inline const FTWeaponDef* FT_GetWeaponDef(const FTWeaponDef aDefs[6], uint8 nSlot)
{
    if(nSlot < 1 || nSlot > 5 || !aDefs[nSlot].sId[0])
    {
        return LTNULL;
    }

    return &aDefs[nSlot];
}

#endif
