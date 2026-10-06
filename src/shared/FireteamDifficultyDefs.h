#ifndef __FIRETEAM_DIFFICULTY_DEFS_H__
#define __FIRETEAM_DIFFICULTY_DEFS_H__

#include <ltbasedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct FTDifficultyDef
{
    char sId[32];

    uint32 nRoundBase;
    uint32 nRoundGrowth;
    uint32 nRoundCap;

    uint32 nMaxAliveBase;
    uint32 nMaxAliveGrowth;
    uint32 nMaxAliveEvery;
    uint32 nMaxAliveCap;

    float fSpawnIntervalBase;
    float fSpawnIntervalDecay;
    float fSpawnIntervalMin;
    float fIntermissionSeconds;

    float fHealthMultiplier;
    float fSpeedMultiplier;
    float fDamageMultiplier;
};

inline char* FT_TrimDifficultyLine(char *pText)
{
    if(!pText) return pText;

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

inline void FT_CopyDifficultyString(
    char *pDest,
    uint32 nDestLen,
    const char *pSrc)
{
    if(!pDest || nDestLen == 0) return;

    if(!pSrc)
    {
        pDest[0] = '\0';
        return;
    }

    strncpy(pDest, pSrc, nDestLen - 1);
    pDest[nDestLen - 1] = '\0';
}

inline void FT_InitDifficultyDefaults(
    FTDifficultyDef &def)
{
    memset(&def, 0, sizeof(def));

    FT_CopyDifficultyString(
        def.sId,
        sizeof(def.sId),
        "normal");

    def.nRoundBase = 6;
    def.nRoundGrowth = 2;
    def.nRoundCap = 36;

    def.nMaxAliveBase = 3;
    def.nMaxAliveGrowth = 1;
    def.nMaxAliveEvery = 2;
    def.nMaxAliveCap = 10;

    def.fSpawnIntervalBase = 1.25f;
    def.fSpawnIntervalDecay = 0.05f;
    def.fSpawnIntervalMin = 0.55f;
    def.fIntermissionSeconds = 5.0f;

    def.fHealthMultiplier = 1.0f;
    def.fSpeedMultiplier = 1.0f;
    def.fDamageMultiplier = 1.0f;
}

inline bool FT_ReadSessionDifficulty(
    const char *pFilename,
    char *pOut,
    uint32 nOutLen)
{
    if(!pOut || nOutLen == 0)
    {
        return false;
    }

    FT_CopyDifficultyString(
        pOut,
        nOutLen,
        "normal");

    FILE *pFile = fopen(pFilename, "rt");
    if(!pFile)
    {
        return false;
    }

    char sLine[256];
    while(fgets(sLine, sizeof(sLine), pFile))
    {
        char *pLine = FT_TrimDifficultyLine(sLine);
        if(!pLine[0] ||
           pLine[0] == '#' ||
           pLine[0] == ';')
        {
            continue;
        }

        char *pEquals = strchr(pLine, '=');
        if(!pEquals)
        {
            continue;
        }

        *pEquals = '\0';
        char *pKey = FT_TrimDifficultyLine(pLine);
        char *pValue = FT_TrimDifficultyLine(pEquals + 1);

        if(_stricmp(pKey, "difficulty") == 0)
        {
            FT_CopyDifficultyString(
                pOut,
                nOutLen,
                pValue);
            fclose(pFile);
            return true;
        }
    }

    fclose(pFile);
    return false;
}

inline bool FT_LoadDifficultyDef(
    const char *pFilename,
    const char *pDifficultyId,
    FTDifficultyDef &def)
{
    FT_InitDifficultyDefaults(def);

    if(!pDifficultyId || !pDifficultyId[0])
    {
        pDifficultyId = "normal";
    }

    FILE *pFile = fopen(pFilename, "rt");
    if(!pFile)
    {
        return false;
    }

    char sLine[512];
    bool bActive = false;
    bool bFound = false;

    while(fgets(sLine, sizeof(sLine), pFile))
    {
        char *pLine = FT_TrimDifficultyLine(sLine);
        if(!pLine[0] ||
           pLine[0] == '#' ||
           pLine[0] == ';')
        {
            continue;
        }

        if(pLine[0] == '[')
        {
            char *pEnd = strchr(pLine, ']');
            if(pEnd)
            {
                *pEnd = '\0';
                bActive =
                    (_stricmp(
                        pLine + 1,
                        pDifficultyId) == 0);

                if(bActive)
                {
                    bFound = true;
                    FT_CopyDifficultyString(
                        def.sId,
                        sizeof(def.sId),
                        pDifficultyId);
                }
            }
            continue;
        }

        if(!bActive)
        {
            continue;
        }

        char *pEquals = strchr(pLine, '=');
        if(!pEquals)
        {
            continue;
        }

        *pEquals = '\0';
        char *pKey = FT_TrimDifficultyLine(pLine);
        char *pValue = FT_TrimDifficultyLine(pEquals + 1);

        if(_stricmp(pKey, "round_base") == 0)
            def.nRoundBase = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "round_growth") == 0)
            def.nRoundGrowth = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "round_cap") == 0)
            def.nRoundCap = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "max_alive_base") == 0)
            def.nMaxAliveBase = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "max_alive_growth") == 0)
            def.nMaxAliveGrowth = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "max_alive_every") == 0)
            def.nMaxAliveEvery = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "max_alive_cap") == 0)
            def.nMaxAliveCap = (uint32)atoi(pValue);
        else if(_stricmp(pKey, "spawn_interval_base") == 0)
            def.fSpawnIntervalBase = (float)atof(pValue);
        else if(_stricmp(pKey, "spawn_interval_decay") == 0)
            def.fSpawnIntervalDecay = (float)atof(pValue);
        else if(_stricmp(pKey, "spawn_interval_min") == 0)
            def.fSpawnIntervalMin = (float)atof(pValue);
        else if(_stricmp(pKey, "intermission") == 0)
            def.fIntermissionSeconds = (float)atof(pValue);
        else if(_stricmp(pKey, "health_multiplier") == 0)
            def.fHealthMultiplier = (float)atof(pValue);
        else if(_stricmp(pKey, "speed_multiplier") == 0)
            def.fSpeedMultiplier = (float)atof(pValue);
        else if(_stricmp(pKey, "damage_multiplier") == 0)
            def.fDamageMultiplier = (float)atof(pValue);
    }

    fclose(pFile);

    if(def.nMaxAliveEvery == 0)
        def.nMaxAliveEvery = 1;

    return bFound;
}

inline bool FT_LoadActiveDifficulty(
    const char *pDifficultyFilename,
    const char *pSessionFilename,
    FTDifficultyDef &def)
{
    char sDifficulty[32];
    FT_ReadSessionDifficulty(
        pSessionFilename,
        sDifficulty,
        sizeof(sDifficulty));

    if(FT_LoadDifficultyDef(
        pDifficultyFilename,
        sDifficulty,
        def))
    {
        return true;
    }

    // A bad/custom session value falls back to Normal.
    return FT_LoadDifficultyDef(
        pDifficultyFilename,
        "normal",
        def);
}

#endif
