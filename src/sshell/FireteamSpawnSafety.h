#ifndef __FIRETEAM_SPAWN_SAFETY_H__
#define __FIRETEAM_SPAWN_SAFETY_H__

// Spawn geometry is authored in DAT world coordinates. The GameStartPoint
// origin is not necessarily a safe PLAYER CENTER for the configured capsule.
// Probe the actual compiled world geometry; do not guess a fixed Y offset.
#include "serverinterfaces.h"
#include <iltserver.h>
#include <ltbasedefs.h>
#include <math.h>

static bool FT_PlayerSpawnGeometryFilter(HOBJECT hObj, void *)
{
    HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
    if(!hClass) return true; // World geometry.

    const char *pIgnore[] = { "CPlayerSrvr", "FireteamZombie", "Seal",
                              "GameStartPoint", "ObjectSpawnPoint" };
    for(uint32 i = 0; i < sizeof(pIgnore) / sizeof(pIgnore[0]); ++i)
    {
        HCLASS hIgnore = g_pLTServer->GetClass(pIgnore[i]);
        if(hIgnore && g_pLTServer->IsKindOf(hClass, hIgnore))
            return false;
    }
    return true;
}

static bool FT_PlayerSpawnTrace(
    const LTVector &vFrom,
    const LTVector &vTo,
    IntersectInfo &info)
{
    IntersectQuery query;
    query.m_From = vFrom;
    query.m_To = vTo;
    query.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID | INTERSECT_HPOLY;
    query.m_FilterFn = FT_PlayerSpawnGeometryFilter;
    query.m_pUserData = LTNULL;
    return g_pLTServer->IntersectSegment(&query, &info);
}

static bool FT_ResolveSafePlayerSpawn(
    const LTVector &vAuthored,
    const LTVector &vHalfDims,
    LTVector &vResolved)
{
    // Offsets are local to the actual authored spawn marker. All tests stay
    // on the same floor; never select a far-away/random zone or upper level.
    const float aOffsets[][2] = {
        { 0.0f, 0.0f },
        { 56.0f, 0.0f }, { -56.0f, 0.0f },
        { 0.0f, 56.0f }, { 0.0f, -56.0f },
        { 56.0f, 56.0f }, { -56.0f, 56.0f },
        { 56.0f, -56.0f }, { -56.0f, -56.0f },
        { 96.0f, 0.0f }, { -96.0f, 0.0f },
        { 0.0f, 96.0f }, { 0.0f, -96.0f }
    };

    const float fHalfX = vHalfDims.x > 12.0f ? vHalfDims.x : 12.0f;
    const float fHalfY = vHalfDims.y > 24.0f ? vHalfDims.y : 24.0f;
    const float fHalfZ = vHalfDims.z > 12.0f ? vHalfDims.z : 12.0f;

    for(uint32 n = 0; n < sizeof(aOffsets)/sizeof(aOffsets[0]); ++n)
    {
        const float fX = vAuthored.x + aOffsets[n][0];
        const float fZ = vAuthored.z + aOffsets[n][1];

        IntersectInfo floorHit;
        LTVector vFrom(fX, vAuthored.y + fHalfY + 18.0f, fZ);
        LTVector vTo(fX, vAuthored.y - 190.0f, fZ);
        if(!FT_PlayerSpawnTrace(vFrom, vTo, floorHit) ||
           floorHit.m_Plane.m_Normal.y < 0.60f)
            continue;

        // Avoid mistaking a ceiling/beam or a lower disconnected floor for
        // the walkable floor belonging to this marker.
        const float fCenterY = floorHit.m_Point.y + fHalfY + 3.0f;
        if(fabs(fCenterY - vAuthored.y) > 95.0f)
            continue;

        LTVector vCandidate(fX, fCenterY, fZ);

        // Vertical headroom test: the whole player height must fit.
        IntersectInfo obstruction;
        LTVector vFeet(fX, floorHit.m_Point.y + 5.0f, fZ);
        LTVector vHead(fX, fCenterY + fHalfY + 8.0f, fZ);
        if(FT_PlayerSpawnTrace(vFeet, vHead, obstruction))
            continue;

        // Keep the player's bounding box off nearby poles, walls and door
        // frames at chest AND head height; a single center ray is not enough.
        const float aRays[][2] = {
            {  fHalfX + 12.0f, 0.0f }, { -fHalfX - 12.0f, 0.0f },
            { 0.0f,  fHalfZ + 12.0f }, { 0.0f, -fHalfZ - 12.0f },
            {  fHalfX + 8.0f,  fHalfZ + 8.0f },
            { -fHalfX - 8.0f, -fHalfZ - 8.0f },
            {  fHalfX + 8.0f, -fHalfZ - 8.0f },
            { -fHalfX - 8.0f,  fHalfZ + 8.0f }
        };
        bool bBlocked = false;
        for(uint32 nHeight = 0; nHeight < 2 && !bBlocked; ++nHeight)
        {
            LTVector vProbe = vCandidate;
            vProbe.y += nHeight == 0 ? -fHalfY * 0.45f : fHalfY * 0.72f;
            for(uint32 i = 0; i < sizeof(aRays)/sizeof(aRays[0]); ++i)
            {
                LTVector vSide = vProbe;
                vSide.x += aRays[i][0];
                vSide.z += aRays[i][1];
                if(FT_PlayerSpawnTrace(vProbe, vSide, obstruction))
                {
                    bBlocked = true;
                    break;
                }
            }
        }
        if(bBlocked)
            continue;

        vResolved = vCandidate;
        g_pLTServer->CPrint(
            "Fireteam spawn safety: authored %.1f %.1f %.1f -> clear %.1f %.1f %.1f (candidate %u).",
            vAuthored.x, vAuthored.y, vAuthored.z,
            vResolved.x, vResolved.y, vResolved.z, n);
        return true;
    }

    g_pLTServer->CPrint(
        "Fireteam spawn safety: NO clear candidate near authored %.1f %.1f %.1f; using map marker unchanged.",
        vAuthored.x, vAuthored.y, vAuthored.z);
    return false;
}

#endif
