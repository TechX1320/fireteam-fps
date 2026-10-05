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
    bool  bShowCrosshair;

    float fProjectileSpeed;
    float fSplashRadius;
    float fFuseSeconds;

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
    memset(aDefs, 0, sizeof(FTWeaponDef) * 6);

    for(uint8 nSlot = 1; nSlot <= 5; ++nSlot)
    {
        aDefs[nSlot].nSlot = nSlot;
        aDefs[nSlot].fDamageMult0 = 1.0f;
        aDefs[nSlot].fDamageMult1 = 1.0f;
        aDefs[nSlot].fDamageMult2 = 1.0f;
        aDefs[nSlot].bShowCrosshair = true;
    }

    // Combat Arms Weapon12 / Ammo1.
    FT_CopyWeaponString(aDefs[1].sId, sizeof(aDefs[1].sId), "ak47");
    FT_CopyWeaponString(aDefs[1].sName, sizeof(aDefs[1].sName), "AK-47");
    aDefs[1].eType = FT_WEAPON_HITSCAN;
    aDefs[1].nClipSize = 30;
    aDefs[1].nStartReserve = 90;
    aDefs[1].nDamage = 48;
    aDefs[1].fFireInterval = 0.10f;
    aDefs[1].fRange = 3500.0f;
    aDefs[1].fEffectRange0 = 2500.0f;
    aDefs[1].fEffectRange1 = 3000.0f;
    aDefs[1].fEffectRange2 = 3500.0f;
    aDefs[1].fDamageMult0 = 1.0f;
    aDefs[1].fDamageMult1 = 0.70f;
    aDefs[1].fDamageMult2 = 0.35f;
    aDefs[1].fReloadSeconds = 2.30f;
    aDefs[1].bAutomatic = true;
    aDefs[1].fViewX = 0.30f;
    aDefs[1].fViewY = -0.60f;
    aDefs[1].fViewZ = 1.20f;
    FT_CopyWeaponString(aDefs[1].sPVModel, sizeof(aDefs[1].sPVModel), "Weapons/primary_m_pv/PV_AR_AK47_SH.LTB");
    FT_CopyWeaponString(aDefs[1].sPVAnim, sizeof(aDefs[1].sPVAnim), "Weapons/primary_m_pv/AK47_ANIBASE.LTB");
    FT_CopyWeaponString(aDefs[1].sPVTexture, sizeof(aDefs[1].sPVTexture), "Weapons/primary_t/AK47_PV.DTX");
    FT_CopyWeaponString(aDefs[1].sHHModel, sizeof(aDefs[1].sHHModel), "Weapons/primary_m_hh/HH_AK-47.LTB");
    FT_CopyWeaponString(aDefs[1].sHHTexture, sizeof(aDefs[1].sHHTexture), "Weapons/primary_t/HH_AK-47.DTX");
    FT_CopyWeaponString(aDefs[1].sSoundDir, sizeof(aDefs[1].sSoundDir), "Weapons/primary_snd/AK47");

    // Combat Arms Weapon24 / Ammo20.
    FT_CopyWeaponString(aDefs[2].sId, sizeof(aDefs[2].sId), "beretta_m92fs");
    FT_CopyWeaponString(aDefs[2].sName, sizeof(aDefs[2].sName), "Beretta M92FS");
    aDefs[2].eType = FT_WEAPON_HITSCAN;
    aDefs[2].nClipSize = 15;
    aDefs[2].nStartReserve = 30;
    aDefs[2].nDamage = 26;
    aDefs[2].fFireInterval = 0.18f;
    aDefs[2].fRange = 2500.0f;
    aDefs[2].fEffectRange0 = 1000.0f;
    aDefs[2].fEffectRange1 = 2000.0f;
    aDefs[2].fEffectRange2 = 2500.0f;
    aDefs[2].fDamageMult0 = 1.0f;
    aDefs[2].fDamageMult1 = 0.70f;
    aDefs[2].fDamageMult2 = 0.35f;
    aDefs[2].fReloadSeconds = 1.65f;
    aDefs[2].bAutomatic = false;
    aDefs[2].fViewX = -0.10f;
    aDefs[2].fViewY = -0.20f;
    aDefs[2].fViewZ = 0.70f;
    FT_CopyWeaponString(aDefs[2].sPVModel, sizeof(aDefs[2].sPVModel), "Weapons/secondary_m_pv/PVMLA_BERETTA_M92FS.LTB");
    FT_CopyWeaponString(aDefs[2].sPVAnim, sizeof(aDefs[2].sPVAnim), "Weapons/secondary_m_pv/COLT_MEU_ANIBASE-1.LTB");
    FT_CopyWeaponString(aDefs[2].sPVTexture, sizeof(aDefs[2].sPVTexture), "Weapons/secondary_t/BERETTA_M92FS_PV.DTX");
    FT_CopyWeaponString(aDefs[2].sHHModel, sizeof(aDefs[2].sHHModel), "Weapons/secondary_m_hh/HH_BERETTA_M92FS.LTB");
    FT_CopyWeaponString(aDefs[2].sHHTexture, sizeof(aDefs[2].sHHTexture), "Weapons/secondary_t/HH_BERETTA_M92FS.DTX");
    FT_CopyWeaponString(aDefs[2].sSoundDir, sizeof(aDefs[2].sSoundDir), "Weapons/secondary_snd/BERETTA_M92FS");

    // Combat Arms Weapon107 / Ammo107.
    FT_CopyWeaponString(aDefs[3].sId, sizeof(aDefs[3].sId), "bowie");
    FT_CopyWeaponString(aDefs[3].sName, sizeof(aDefs[3].sName), "Bowie Knife");
    aDefs[3].eType = FT_WEAPON_MELEE;
    aDefs[3].nDamage = 70;
    aDefs[3].fFireInterval = 0.55f;
    aDefs[3].fRange = 135.0f;
    aDefs[3].fEffectRange0 = 135.0f;
    aDefs[3].fEffectRange1 = 135.0f;
    aDefs[3].fEffectRange2 = 135.0f;
    aDefs[3].bShowCrosshair = false;
    FT_CopyWeaponString(aDefs[3].sPVModel, sizeof(aDefs[3].sPVModel), "Weapons/melee_m_pv/CM_HND_NM_DF_BOWIEKNIFE_CH.LTB");
    FT_CopyWeaponString(aDefs[3].sPVAnim, sizeof(aDefs[3].sPVAnim), "Weapons/melee_m_pv/ANI_G_BOWIEKNIFE_CH.LTB");
    FT_CopyWeaponString(aDefs[3].sPVTexture, sizeof(aDefs[3].sPVTexture), "Weapons/melee_t/PV_ML_DF_BOWIEKNIFE_BC.DTX");
    FT_CopyWeaponString(aDefs[3].sHHModel, sizeof(aDefs[3].sHHModel), "Weapons/melee_m_hh/HH_ML_DF_BOWIEKNIFE_CH.LTB");
    FT_CopyWeaponString(aDefs[3].sHHTexture, sizeof(aDefs[3].sHHTexture), "Weapons/melee_t/HH_ML_DF_BOWIEKNIFE_BC.DTX");
    FT_CopyWeaponString(aDefs[3].sSoundDir, sizeof(aDefs[3].sSoundDir), "Weapons/melee_snd/BOWIE_KNIFE");

    // Combat Arms Weapon7 / Ammo9. Projectile speed/fuse are DEV timing.
    FT_CopyWeaponString(aDefs[4].sId, sizeof(aDefs[4].sId), "m67");
    FT_CopyWeaponString(aDefs[4].sName, sizeof(aDefs[4].sName), "M67 Frag Grenade");
    aDefs[4].eType = FT_WEAPON_GRENADE;
    aDefs[4].nClipSize = 1;
    aDefs[4].nStartReserve = 0;
    aDefs[4].nDamage = 60;
    aDefs[4].fFireInterval = 0.80f;
    aDefs[4].fRange = 450.0f;
    aDefs[4].fEffectRange0 = 450.0f;
    aDefs[4].fEffectRange1 = 450.0f;
    aDefs[4].fEffectRange2 = 450.0f;
    aDefs[4].fDamageMult0 = 1.0f;
    aDefs[4].fDamageMult1 = 0.80f;
    aDefs[4].fDamageMult2 = 0.50f;
    aDefs[4].fProjectileSpeed = 650.0f;
    aDefs[4].fSplashRadius = 450.0f;
    aDefs[4].fFuseSeconds = 3.0f;
    aDefs[4].fViewX = 0.50f;
    aDefs[4].fViewY = -0.90f;
    aDefs[4].fViewZ = 1.50f;
    FT_CopyWeaponString(aDefs[4].sPVModel, sizeof(aDefs[4].sPVModel), "Weapons/grenade_m_pv/PVMLA_M67.LTB");
    FT_CopyWeaponString(aDefs[4].sPVAnim, sizeof(aDefs[4].sPVAnim), "Weapons/grenade_m_pv/M67_ANIBASE-1.LTB");
    FT_CopyWeaponString(aDefs[4].sPVTexture, sizeof(aDefs[4].sPVTexture), "Weapons/grenade_t/M67_PV.DTX");
    FT_CopyWeaponString(aDefs[4].sHHModel, sizeof(aDefs[4].sHHModel), "Weapons/grenade_m_hh/HH_M67.LTB");
    FT_CopyWeaponString(aDefs[4].sHHTexture, sizeof(aDefs[4].sHHTexture), "Weapons/grenade_t/HH_M67.DTX");
    FT_CopyWeaponString(aDefs[4].sSoundDir, sizeof(aDefs[4].sSoundDir), "Weapons/grenade_snd/M67");

    // Combat Arms Weapon29 / Ammo29. Projectile speed/reload timing are DEV.
    FT_CopyWeaponString(aDefs[5].sId, sizeof(aDefs[5].sId), "law");
    FT_CopyWeaponString(aDefs[5].sName, sizeof(aDefs[5].sName), "LAW");
    aDefs[5].eType = FT_WEAPON_ROCKET;
    aDefs[5].nClipSize = 1;
    aDefs[5].nStartReserve = 2;
    aDefs[5].nDamage = 40;
    aDefs[5].fFireInterval = 1.0f;
    aDefs[5].fRange = 5000.0f;
    aDefs[5].fEffectRange0 = 2500.0f;
    aDefs[5].fEffectRange1 = 4000.0f;
    aDefs[5].fEffectRange2 = 5000.0f;
    aDefs[5].fDamageMult0 = 0.0f;
    aDefs[5].fDamageMult1 = 0.0f;
    aDefs[5].fDamageMult2 = 0.0f;
    aDefs[5].fReloadSeconds = 2.80f;
    aDefs[5].fProjectileSpeed = 1600.0f;
    aDefs[5].fSplashRadius = 550.0f;
    aDefs[5].fFuseSeconds = 8.0f;
    aDefs[5].fViewX = 0.30f;
    aDefs[5].fViewY = -0.60f;
    aDefs[5].fViewZ = 1.80f;
    FT_CopyWeaponString(aDefs[5].sPVModel, sizeof(aDefs[5].sPVModel), "Weapons/special_m_pv/PVMLA_LAW.LTB");
    FT_CopyWeaponString(aDefs[5].sPVAnim, sizeof(aDefs[5].sPVAnim), "Weapons/special_m_pv/LAW_ANIBASE.LTB");
    FT_CopyWeaponString(aDefs[5].sPVTexture, sizeof(aDefs[5].sPVTexture), "Weapons/special_t/LAW_PV.DTX");
    FT_CopyWeaponString(aDefs[5].sHHModel, sizeof(aDefs[5].sHHModel), "Weapons/special_m_hh/HH_LAW.LTB");
    FT_CopyWeaponString(aDefs[5].sHHTexture, sizeof(aDefs[5].sHHTexture), "Weapons/special_t/HH_LAW.DTX");
    FT_CopyWeaponString(aDefs[5].sSoundDir, sizeof(aDefs[5].sSoundDir), "Weapons/special_snd/LAW");
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
        else if(_stricmp(pKey, "show_crosshair") == 0) def.bShowCrosshair = atoi(pValue) != 0;
        else if(_stricmp(pKey, "projectile_speed") == 0) def.fProjectileSpeed = (float)atof(pValue);
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
