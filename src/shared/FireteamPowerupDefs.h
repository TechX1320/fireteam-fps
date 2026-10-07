#ifndef __FIRETEAM_POWERUP_DEFS_H__
#define __FIRETEAM_POWERUP_DEFS_H__

#include <ltbasedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct FTPowerupDef
{
    bool bEnabled;

    char sModel[128];
    char sTexture[128];
    char sRenderStyle[128];
    char sPickupSound[128];

    uint32 nKillDropChancePercent;
    uint32 nRoundClearMin;
    uint32 nRoundClearMax;

    uint32 nAmmoWeight;
    uint32 nHealthWeight;
    uint32 nBottomlessWeight;
    uint32 nOneHitWeight;
    uint32 nGodWeight;
    uint32 nWallhackWeight;

    uint32 nAmmoMagazines;
    uint32 nHealthAmount;

    float fBottomlessSeconds;
    float fOneHitSeconds;
    float fGodSeconds;
    float fWallhackSeconds;
    float fLifetimeSeconds;
};

inline char* FT_TrimPowerup(char *pText)
{
    if(!pText) return pText;

    while(*pText &&
          isspace((unsigned char)*pText))
    {
        ++pText;
    }

    char *pEnd =
        pText + strlen(pText);

    while(pEnd > pText &&
          isspace((unsigned char)pEnd[-1]))
    {
        --pEnd;
    }

    *pEnd = '\0';
    return pText;
}

inline void FT_CopyPowerupString(
    char *pDest,
    uint32 nDestLen,
    const char *pSource)
{
    if(!pDest || nDestLen == 0)
        return;

    if(!pSource)
    {
        pDest[0] = '\0';
        return;
    }

    strncpy(
        pDest,
        pSource,
        nDestLen - 1);

    pDest[nDestLen - 1] = '\0';
}

inline void FT_InitPowerupDef(
    FTPowerupDef &def)
{
    memset(
        &def,
        0,
        sizeof(def));

    def.bEnabled = true;
    def.nKillDropChancePercent = 10;
    def.nRoundClearMin = 0;
    def.nRoundClearMax = 4;

    def.nAmmoWeight = 45;
    def.nHealthWeight = 25;
    def.nBottomlessWeight = 15;
    def.nOneHitWeight = 15;
    def.nGodWeight = 2;
    def.nWallhackWeight = 3;

    def.nAmmoMagazines = 2;
    def.nHealthAmount = 35;

    def.fBottomlessSeconds = 30.0f;
    def.fOneHitSeconds = 30.0f;
    def.fGodSeconds = 10.0f;
    def.fWallhackSeconds = 20.0f;
    def.fLifetimeSeconds = 25.0f;

    FT_CopyPowerupString(
        def.sModel,
        sizeof(def.sModel),
        "FX/MUTATION/SND/MUTATION_BUFF_SH.LTB");

    FT_CopyPowerupString(
        def.sTexture,
        sizeof(def.sTexture),
        "FX/MUTATION/SND/MUTATION_WHITE_SH.DTX");

    FT_CopyPowerupString(
        def.sRenderStyle,
        sizeof(def.sRenderStyle),
        "RenderStyles/default.ltb");

    FT_CopyPowerupString(
        def.sPickupSound,
        sizeof(def.sPickupSound),
        "FX/MUTATION/SND/GET.WAV");
}

inline bool FT_LoadPowerupDef(
    const char *pFilename,
    FTPowerupDef &def)
{
    FT_InitPowerupDef(def);

    FILE *pFile =
        fopen(pFilename, "rt");

    if(!pFile)
    {
        return false;
    }

    bool bInSection = false;
    char sLine[512];

    while(fgets(
        sLine,
        sizeof(sLine),
        pFile))
    {
        char *pLine =
            FT_TrimPowerup(sLine);

        if(!pLine[0] ||
           pLine[0] == '#' ||
           pLine[0] == ';')
        {
            continue;
        }

        if(pLine[0] == '[')
        {
            char *pEnd =
                strchr(pLine, ']');

            if(pEnd)
            {
                *pEnd = '\0';
                bInSection =
                    _stricmp(
                        pLine + 1,
                        "mutation_box") == 0;
            }

            continue;
        }

        if(!bInSection)
            continue;

        char *pEquals =
            strchr(pLine, '=');

        if(!pEquals)
            continue;

        *pEquals = '\0';

        char *pKey =
            FT_TrimPowerup(pLine);

        char *pValue =
            FT_TrimPowerup(
                pEquals + 1);

        if(_stricmp(pKey, "enabled") == 0)
            def.bEnabled = atoi(pValue) != 0;
        else if(_stricmp(pKey, "model") == 0)
            FT_CopyPowerupString(def.sModel, sizeof(def.sModel), pValue);
        else if(_stricmp(pKey, "texture") == 0)
            FT_CopyPowerupString(def.sTexture, sizeof(def.sTexture), pValue);
        else if(_stricmp(pKey, "renderstyle") == 0)
            FT_CopyPowerupString(def.sRenderStyle, sizeof(def.sRenderStyle), pValue);
        else if(_stricmp(pKey, "pickup_sound") == 0)
            FT_CopyPowerupString(def.sPickupSound, sizeof(def.sPickupSound), pValue);
        else if(_stricmp(pKey, "kill_drop_chance") == 0)
            def.nKillDropChancePercent = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "round_clear_min") == 0)
            def.nRoundClearMin = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "round_clear_max") == 0)
            def.nRoundClearMax = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "ammo_weight") == 0)
            def.nAmmoWeight = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "health_weight") == 0)
            def.nHealthWeight = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "bottomless_weight") == 0)
            def.nBottomlessWeight = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "one_hit_weight") == 0)
            def.nOneHitWeight = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "god_weight") == 0)
            def.nGodWeight = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "wallhack_weight") == 0)
            def.nWallhackWeight = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "ammo_magazines") == 0)
            def.nAmmoMagazines = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "health_amount") == 0)
            def.nHealthAmount = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "bottomless_seconds") == 0)
            def.fBottomlessSeconds = (float)atof(pValue);
        else if(_stricmp(pKey, "one_hit_seconds") == 0)
            def.fOneHitSeconds = (float)atof(pValue);
        else if(_stricmp(pKey, "god_seconds") == 0)
            def.fGodSeconds = (float)atof(pValue);
        else if(_stricmp(pKey, "wallhack_seconds") == 0)
            def.fWallhackSeconds = (float)atof(pValue);
        else if(_stricmp(pKey, "lifetime_seconds") == 0)
            def.fLifetimeSeconds = (float)atof(pValue);
    }

    fclose(pFile);
    return true;
}

#endif
