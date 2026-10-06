#ifndef __FIRETEAM_INFECTED_DEFS_H__
#define __FIRETEAM_INFECTED_DEFS_H__

#include <ltbasedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct FTInfectedDef
{
    char sId[32];
    char sName[64];

    uint16 nHealth;
    float fRunSpeed;
    uint8 nAttackDamage;
    float fAttackRange;
    float fAttackCooldown;
    float fUpdateSeconds;

    char sCollisionMode[32];
    float fCollisionX;
    float fCollisionY;
    float fCollisionZ;

    char sBodyModel[128];
    char sAnimationModel[128];
    char sBodyTexture0[128];
    char sBodyTexture1[128];
    char sBodyRenderStyle0[128];
    char sBodyRenderStyle1[128];

    char sFaceMode[32];
    char sFaceModel[128];
    char sFaceTexture[128];
    char sFaceRenderStyle0[128];
    char sFaceRenderStyle1[128];
    char sFaceSocket[64];
    char sFaceAlignNode[64];
    bool bFaceAutoAlign;

    // Optional extra modular head/headgear for custom characters.
    char sHeadModel[128];
    char sHeadTexture[128];

    float fFacePosX;
    float fFacePosY;
    float fFacePosZ;
    float fFaceRotX;
    float fFaceRotY;
    float fFaceRotZ;

    char sIdleAnim[64];
};

inline void FT_CopyInfectedString(
    char *pDest,
    uint32 nDestLen,
    const char *pSrc)
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

inline char* FT_TrimInfectedLine(char *pText)
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

inline void FT_InitInfectedDef(FTInfectedDef &def)
{
    memset(&def, 0, sizeof(def));
}

inline void FT_ApplyInfectedValue(
    FTInfectedDef &def,
    const char *pKey,
    const char *pValue)
{
    if(_stricmp(pKey, "id") == 0)
        FT_CopyInfectedString(def.sId, sizeof(def.sId), pValue);
    else if(_stricmp(pKey, "name") == 0)
        FT_CopyInfectedString(def.sName, sizeof(def.sName), pValue);
    else if(_stricmp(pKey, "health") == 0)
        def.nHealth = (uint16)atoi(pValue);
    else if(_stricmp(pKey, "run_speed") == 0)
        def.fRunSpeed = (float)atof(pValue);
    else if(_stricmp(pKey, "attack_damage") == 0)
        def.nAttackDamage = (uint8)atoi(pValue);
    else if(_stricmp(pKey, "attack_range") == 0)
        def.fAttackRange = (float)atof(pValue);
    else if(_stricmp(pKey, "attack_cooldown") == 0)
        def.fAttackCooldown = (float)atof(pValue);
    else if(_stricmp(pKey, "update_seconds") == 0)
        def.fUpdateSeconds = (float)atof(pValue);
    else if(_stricmp(pKey, "collision_mode") == 0)
        FT_CopyInfectedString(def.sCollisionMode, sizeof(def.sCollisionMode), pValue);
    else if(_stricmp(pKey, "collision_x") == 0)
        def.fCollisionX = (float)atof(pValue);
    else if(_stricmp(pKey, "collision_y") == 0)
        def.fCollisionY = (float)atof(pValue);
    else if(_stricmp(pKey, "collision_z") == 0)
        def.fCollisionZ = (float)atof(pValue);
    else if(_stricmp(pKey, "body_model") == 0)
        FT_CopyInfectedString(def.sBodyModel, sizeof(def.sBodyModel), pValue);
    else if(_stricmp(pKey, "animation_model") == 0)
        FT_CopyInfectedString(def.sAnimationModel, sizeof(def.sAnimationModel), pValue);
    else if(_stricmp(pKey, "body_texture0") == 0)
        FT_CopyInfectedString(def.sBodyTexture0, sizeof(def.sBodyTexture0), pValue);
    else if(_stricmp(pKey, "body_texture1") == 0)
        FT_CopyInfectedString(def.sBodyTexture1, sizeof(def.sBodyTexture1), pValue);
    else if(_stricmp(pKey, "body_renderstyle0") == 0)
        FT_CopyInfectedString(def.sBodyRenderStyle0, sizeof(def.sBodyRenderStyle0), pValue);
    else if(_stricmp(pKey, "body_renderstyle1") == 0)
        FT_CopyInfectedString(def.sBodyRenderStyle1, sizeof(def.sBodyRenderStyle1), pValue);
    else if(_stricmp(pKey, "face_mode") == 0)
        FT_CopyInfectedString(def.sFaceMode, sizeof(def.sFaceMode), pValue);
    else if(_stricmp(pKey, "face_model") == 0)
        FT_CopyInfectedString(def.sFaceModel, sizeof(def.sFaceModel), pValue);
    else if(_stricmp(pKey, "face_texture") == 0)
        FT_CopyInfectedString(def.sFaceTexture, sizeof(def.sFaceTexture), pValue);
    else if(_stricmp(pKey, "face_renderstyle0") == 0)
        FT_CopyInfectedString(def.sFaceRenderStyle0, sizeof(def.sFaceRenderStyle0), pValue);
    else if(_stricmp(pKey, "face_renderstyle1") == 0)
        FT_CopyInfectedString(def.sFaceRenderStyle1, sizeof(def.sFaceRenderStyle1), pValue);
    else if(_stricmp(pKey, "face_socket") == 0)
        FT_CopyInfectedString(def.sFaceSocket, sizeof(def.sFaceSocket), pValue);
    else if(_stricmp(pKey, "face_align_node") == 0)
        FT_CopyInfectedString(def.sFaceAlignNode, sizeof(def.sFaceAlignNode), pValue);
    else if(_stricmp(pKey, "face_auto_align") == 0)
        def.bFaceAutoAlign = atoi(pValue) != 0;
    else if(_stricmp(pKey, "head_model") == 0)
        FT_CopyInfectedString(def.sHeadModel, sizeof(def.sHeadModel), pValue);
    else if(_stricmp(pKey, "head_texture") == 0)
        FT_CopyInfectedString(def.sHeadTexture, sizeof(def.sHeadTexture), pValue);
    else if(_stricmp(pKey, "face_pos_x") == 0)
        def.fFacePosX = (float)atof(pValue);
    else if(_stricmp(pKey, "face_pos_y") == 0)
        def.fFacePosY = (float)atof(pValue);
    else if(_stricmp(pKey, "face_pos_z") == 0)
        def.fFacePosZ = (float)atof(pValue);
    else if(_stricmp(pKey, "face_rot_x") == 0)
        def.fFaceRotX = (float)atof(pValue);
    else if(_stricmp(pKey, "face_rot_y") == 0)
        def.fFaceRotY = (float)atof(pValue);
    else if(_stricmp(pKey, "face_rot_z") == 0)
        def.fFaceRotZ = (float)atof(pValue);
    else if(_stricmp(pKey, "idle_anim") == 0)
        FT_CopyInfectedString(def.sIdleAnim, sizeof(def.sIdleAnim), pValue);
}

inline bool FT_LoadInfectedSection(
    const char *pFilename,
    const char *pSectionName,
    FTInfectedDef &def)
{
    FILE *pFile = fopen(pFilename, "rt");
    if(!pFile)
    {
        return false;
    }

    char sLine[512];
    char sSection[64];
    sSection[0] = '\0';
    bool bFound = false;

    while(fgets(sLine, sizeof(sLine), pFile))
    {
        char *pLine = FT_TrimInfectedLine(sLine);
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
                FT_CopyInfectedString(
                    sSection,
                    sizeof(sSection),
                    pLine + 1);

                bFound =
                    (_stricmp(
                        sSection,
                        pSectionName) == 0);
            }
            continue;
        }

        if(!bFound)
        {
            continue;
        }

        char *pEquals = strchr(pLine, '=');
        if(!pEquals)
        {
            continue;
        }

        *pEquals = '\0';
        char *pKey = FT_TrimInfectedLine(pLine);
        char *pValue = FT_TrimInfectedLine(pEquals + 1);

        FT_ApplyInfectedValue(
            def,
            pKey,
            pValue);
    }

    fclose(pFile);
    return true;
}

inline bool FT_LoadDefaultInfectedDef(
    const char *pFilename,
    FTInfectedDef &def)
{
    FT_InitInfectedDef(def);

    FILE *pFile = fopen(pFilename, "rt");
    if(!pFile)
    {
        return false;
    }

    char sDefaultSection[64];
    sDefaultSection[0] = '\0';

    char sLine[512];
    char sSection[64];
    sSection[0] = '\0';

    while(fgets(sLine, sizeof(sLine), pFile))
    {
        char *pLine = FT_TrimInfectedLine(sLine);
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
                FT_CopyInfectedString(
                    sSection,
                    sizeof(sSection),
                    pLine + 1);
            }
            continue;
        }

        if(_stricmp(sSection, "settings") != 0)
        {
            continue;
        }

        char *pEquals = strchr(pLine, '=');
        if(!pEquals)
        {
            continue;
        }

        *pEquals = '\0';
        char *pKey = FT_TrimInfectedLine(pLine);
        char *pValue = FT_TrimInfectedLine(pEquals + 1);

        if(_stricmp(pKey, "default") == 0)
        {
            FT_CopyInfectedString(
                sDefaultSection,
                sizeof(sDefaultSection),
                pValue);
        }
    }

    fclose(pFile);

    if(!sDefaultSection[0])
    {
        return false;
    }

    FT_LoadInfectedSection(
        pFilename,
        sDefaultSection,
        def);

    // Character/model presentation is deliberately a separate file so users
    // can fix/replace bodies, faces and offsets without touching AI balance.
    FT_LoadInfectedSection(
        "config/characters.cfg",
        sDefaultSection,
        def);

    return def.sId[0] &&
           def.sBodyModel[0] &&
           def.nHealth > 0 &&
           def.fRunSpeed > 0.0f;
}

#endif
