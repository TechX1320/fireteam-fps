#ifndef __FIRETEAM_PLAYER_DEFS_H__
#define __FIRETEAM_PLAYER_DEFS_H__

#include <ltbasedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct FTPlayerDef
{
    char sBodyModel[128];
    char sAnimationModel[128];
    char sSkin0[128];
    char sSkin1[128];
    char sFirstPersonHandTexture[128];

    float fCollisionX;
    float fCollisionY;
    float fCollisionZ;
};

inline char* FT_TrimPlayerLine(char *pText)
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

inline void FT_CopyPlayerString(
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

inline bool FT_LoadPlayerDef(
    const char *pFilename,
    FTPlayerDef &def)
{
    memset(&def, 0, sizeof(def));

    FILE *pFile = fopen(pFilename, "rt");
    if(!pFile)
    {
        return false;
    }

    char sLine[512];
    bool bPlayerSection = false;

    while(fgets(sLine, sizeof(sLine), pFile))
    {
        char *pLine = FT_TrimPlayerLine(sLine);
        if(!pLine[0] || pLine[0] == '#' || pLine[0] == ';')
        {
            continue;
        }

        if(pLine[0] == '[')
        {
            char *pEnd = strchr(pLine, ']');
            if(pEnd)
            {
                *pEnd = '\0';
                bPlayerSection =
                    (_stricmp(pLine + 1, "player") == 0);
            }
            continue;
        }

        if(!bPlayerSection)
        {
            continue;
        }

        char *pEquals = strchr(pLine, '=');
        if(!pEquals)
        {
            continue;
        }

        *pEquals = '\0';
        char *pKey = FT_TrimPlayerLine(pLine);
        char *pValue = FT_TrimPlayerLine(pEquals + 1);

        if(_stricmp(pKey, "body_model") == 0)
            FT_CopyPlayerString(def.sBodyModel, sizeof(def.sBodyModel), pValue);
        else if(_stricmp(pKey, "animation_model") == 0)
            FT_CopyPlayerString(def.sAnimationModel, sizeof(def.sAnimationModel), pValue);
        else if(_stricmp(pKey, "skin0") == 0)
            FT_CopyPlayerString(def.sSkin0, sizeof(def.sSkin0), pValue);
        else if(_stricmp(pKey, "skin1") == 0)
            FT_CopyPlayerString(def.sSkin1, sizeof(def.sSkin1), pValue);
        else if(_stricmp(pKey, "first_person_hand_texture") == 0)
            FT_CopyPlayerString(def.sFirstPersonHandTexture, sizeof(def.sFirstPersonHandTexture), pValue);
        else if(_stricmp(pKey, "collision_x") == 0)
            def.fCollisionX = (float)atof(pValue);
        else if(_stricmp(pKey, "collision_y") == 0)
            def.fCollisionY = (float)atof(pValue);
        else if(_stricmp(pKey, "collision_z") == 0)
            def.fCollisionZ = (float)atof(pValue);
    }

    fclose(pFile);

    return def.sBodyModel[0] &&
           def.fCollisionX > 0.0f &&
           def.fCollisionY > 0.0f &&
           def.fCollisionZ > 0.0f;
}

#endif
