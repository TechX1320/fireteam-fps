param(
    [string]$LocalRoot = (Join-Path $PSScriptRoot '..\.local')
)

$ErrorActionPreference = 'Stop'

function Write-Source([string]$Path, [string]$Text) {
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    [IO.File]::WriteAllText($Path, $Text, [Text.Encoding]::ASCII)
}

$sealRoot = Join-Path $LocalRoot 'imports\sealhunter'
if (-not (Test-Path -LiteralPath (Join-Path $sealRoot 'sshell\src\playersrvr.cpp'))) {
    throw "SealHunter source is missing under '$sealRoot'."
}

$navigationH = @'
#ifndef __FIRETEAM_NAVIGATION_H__
#define __FIRETEAM_NAVIGATION_H__

#include <ltengineobjects.h>
#include <vector>

class AIRegion : public BaseClass
{
public:
    AIRegion();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);
    void PrintNavigationSummary();

    char m_sName[64];
    LTVector m_vDims;
};

class AINodePatrol : public BaseClass
{
public:
    AINodePatrol();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);

    char m_sName[64];
    char m_sNext[64];
    bool m_bStartDisabled;
};

bool FT_BuildNavigationPath(
    const LTVector &vStart,
    const LTVector &vDestination,
    uint32 nLane,
    std::vector<LTVector> &aWaypoints);

uint32 FT_GetNavigationVolumeCount();

#endif
'@

$navigationCpp = @'
#include "FireteamNavigation.h"
#include "AIVolume.h"
#include "serverinterfaces.h"

#include <ltobjectcreate.h>
#include <float.h>
#include <math.h>
#include <string.h>

BEGIN_CLASS(AIRegion)
    ADD_VECTORPROP_VAL_FLAG(Dims, 16.0f, 16.0f, 16.0f, PF_DIMS)
    ADD_BOOLPROP(Key1, LTFALSE)
    ADD_BOOLPROP(Key2, LTFALSE)
    ADD_BOOLPROP(Key3, LTFALSE)
    ADD_BOOLPROP(Key4, LTFALSE)
    ADD_BOOLPROP(Key5, LTFALSE)
    ADD_BOOLPROP(Key6, LTFALSE)
    ADD_BOOLPROP(Key7, LTFALSE)
    ADD_BOOLPROP(Key8, LTFALSE)
END_CLASS_DEFAULT_FLAGS(AIRegion, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

BEGIN_CLASS(AINodePatrol)
    ADD_VECTORPROP_VAL_FLAG(Dims, 16.0f, 16.0f, 16.0f, PF_DIMS)
    ADD_BOOLPROP(Face, LTTRUE)
    ADD_STRINGPROP(Alignment, "None")
    ADD_BOOLPROP(StartDisabled, LTFALSE)
    ADD_STRINGPROP_FLAG(Next, "", PF_OBJECTLINK)
    ADD_STRINGPROP(Action, "")
    ADD_STRINGPROP(Command, "")
END_CLASS_DEFAULT_FLAGS(AINodePatrol, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

struct FTNavVolume
{
    AIVolume *pVolume;
    LTVector vCenter;
    LTVector vDims;
};

static const uint32 kMaxNavVolumes = 256;

static float FTMax(float a, float b) { return a > b ? a : b; }
static float FTMin(float a, float b) { return a < b ? a : b; }

static uint32 FTCollectVolumes(FTNavVolume *pVolumes, uint32 nMax)
{
    HCLASS hVolumeClass = g_pLTServer->GetClass("AIVolume");
    if(!hVolumeClass)
    {
        return 0;
    }

    uint32 nCount = 0;
    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj && nCount < nMax;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(!hClass || !g_pLTServer->IsKindOf(hClass, hVolumeClass))
        {
            continue;
        }

        AIVolume *pVolume = (AIVolume*)g_pLTServer->HandleToObject(hObj);
        if(!pVolume)
        {
            continue;
        }

        pVolumes[nCount].pVolume = pVolume;
        pVolumes[nCount].vDims = pVolume->GetDims();
        g_pLTServer->GetObjectPos(hObj, &pVolumes[nCount].vCenter);
        ++nCount;
    }

    return nCount;
}

static int FTFindVolume(const FTNavVolume *pVolumes, uint32 nCount, const LTVector &vPos)
{
    int nBest = -1;
    float fBest = FLT_MAX;

    for(uint32 i = 0; i < nCount; ++i)
    {
        const LTVector &c = pVolumes[i].vCenter;
        const LTVector &d = pVolumes[i].vDims;

        float dx = (float)fabs(vPos.x - c.x) - d.x;
        float dz = (float)fabs(vPos.z - c.z) - d.z;
        float dy = (float)fabs(vPos.y - c.y) - (d.y + 96.0f);

        if(dx <= 6.0f && dz <= 6.0f && dy <= 0.0f)
        {
            float fArea = FTMax(1.0f, d.x * d.z);
            if(fArea < fBest)
            {
                fBest = fArea;
                nBest = (int)i;
            }
        }
    }

    if(nBest >= 0)
    {
        return nBest;
    }

    fBest = FLT_MAX;
    for(uint32 i = 0; i < nCount; ++i)
    {
        const LTVector &c = pVolumes[i].vCenter;
        const LTVector &d = pVolumes[i].vDims;

        float dx = FTMax(0.0f, (float)fabs(vPos.x - c.x) - d.x);
        float dz = FTMax(0.0f, (float)fabs(vPos.z - c.z) - d.z);
        float dy = FTMax(0.0f, (float)fabs(vPos.y - c.y) - (d.y + 96.0f));
        float fScore = (dx * dx) + (dz * dz) + (dy * dy * 0.25f);

        if(fScore < fBest)
        {
            fBest = fScore;
            nBest = (int)i;
        }
    }

    return nBest;
}

static bool FTGetConnection(
    const FTNavVolume &a,
    const FTNavVolume &b,
    uint32 nLane,
    LTVector *pOut)
{
    const float kSlack = 18.0f;
    const float kVerticalSlack = 96.0f;
    const float kMinOpening = 24.0f;

    float aMinX = a.vCenter.x - a.vDims.x;
    float aMaxX = a.vCenter.x + a.vDims.x;
    float aMinY = a.vCenter.y - a.vDims.y;
    float aMaxY = a.vCenter.y + a.vDims.y;
    float aMinZ = a.vCenter.z - a.vDims.z;
    float aMaxZ = a.vCenter.z + a.vDims.z;

    float bMinX = b.vCenter.x - b.vDims.x;
    float bMaxX = b.vCenter.x + b.vDims.x;
    float bMinY = b.vCenter.y - b.vDims.y;
    float bMaxY = b.vCenter.y + b.vDims.y;
    float bMinZ = b.vCenter.z - b.vDims.z;
    float bMaxZ = b.vCenter.z + b.vDims.z;

    float overlapXMin = FTMax(aMinX, bMinX);
    float overlapXMax = FTMin(aMaxX, bMaxX);
    float overlapZMin = FTMax(aMinZ, bMinZ);
    float overlapZMax = FTMin(aMaxZ, bMaxZ);
    float overlapX = overlapXMax - overlapXMin;
    float overlapZ = overlapZMax - overlapZMin;

    float gapX = 0.0f;
    if(aMaxX < bMinX) gapX = bMinX - aMaxX;
    else if(bMaxX < aMinX) gapX = aMinX - bMaxX;

    float gapZ = 0.0f;
    if(aMaxZ < bMinZ) gapZ = bMinZ - aMaxZ;
    else if(bMaxZ < aMinZ) gapZ = aMinZ - bMaxZ;

    float gapY = 0.0f;
    if(aMaxY < bMinY) gapY = bMinY - aMaxY;
    else if(bMaxY < aMinY) gapY = aMinY - bMaxY;

    if(gapY > kVerticalSlack)
    {
        return false;
    }

    float fLane = 0.5f;
    if((nLane % 3) == 0) fLane = 0.28f;
    else if((nLane % 3) == 2) fLane = 0.72f;

    if(gapX <= kSlack && overlapZ >= kMinOpening)
    {
        if(overlapX >= 0.0f)
            pOut->x = (overlapXMin + overlapXMax) * 0.5f;
        else if(a.vCenter.x < b.vCenter.x)
            pOut->x = (aMaxX + bMinX) * 0.5f;
        else
            pOut->x = (bMaxX + aMinX) * 0.5f;

        pOut->z = overlapZMin + ((overlapZMax - overlapZMin) * fLane);
        pOut->y = (a.vCenter.y + b.vCenter.y) * 0.5f;
        return true;
    }

    if(gapZ <= kSlack && overlapX >= kMinOpening)
    {
        if(overlapZ >= 0.0f)
            pOut->z = (overlapZMin + overlapZMax) * 0.5f;
        else if(a.vCenter.z < b.vCenter.z)
            pOut->z = (aMaxZ + bMinZ) * 0.5f;
        else
            pOut->z = (bMaxZ + aMinZ) * 0.5f;

        pOut->x = overlapXMin + ((overlapXMax - overlapXMin) * fLane);
        pOut->y = (a.vCenter.y + b.vCenter.y) * 0.5f;
        return true;
    }

    if(overlapX > kMinOpening && overlapZ > kMinOpening)
    {
        pOut->x = overlapXMin + ((overlapXMax - overlapXMin) * fLane);
        pOut->z = overlapZMin + ((overlapZMax - overlapZMin) * (1.0f - fLane));
        pOut->y = (a.vCenter.y + b.vCenter.y) * 0.5f;
        return true;
    }

    return false;
}

uint32 FT_GetNavigationVolumeCount()
{
    FTNavVolume aVolumes[kMaxNavVolumes];
    return FTCollectVolumes(aVolumes, kMaxNavVolumes);
}

bool FT_BuildNavigationPath(
    const LTVector &vStart,
    const LTVector &vDestination,
    uint32 nLane,
    std::vector<LTVector> &aWaypoints)
{
    aWaypoints.clear();

    FTNavVolume aVolumes[kMaxNavVolumes];
    uint32 nCount = FTCollectVolumes(aVolumes, kMaxNavVolumes);
    if(nCount == 0)
    {
        return false;
    }

    int nSource = FTFindVolume(aVolumes, nCount, vStart);
    int nDest = FTFindVolume(aVolumes, nCount, vDestination);
    if(nSource < 0 || nDest < 0)
    {
        return false;
    }

    if(nSource == nDest)
    {
        aWaypoints.push_back(vDestination);
        return true;
    }

    float aDistance[kMaxNavVolumes];
    int aPrevious[kMaxNavVolumes];
    bool aVisited[kMaxNavVolumes];

    for(uint32 i = 0; i < nCount; ++i)
    {
        aDistance[i] = FLT_MAX;
        aPrevious[i] = -1;
        aVisited[i] = false;
    }

    aDistance[nSource] = 0.0f;

    for(uint32 nStep = 0; nStep < nCount; ++nStep)
    {
        int nCurrent = -1;
        float fCurrent = FLT_MAX;

        for(uint32 i = 0; i < nCount; ++i)
        {
            if(!aVisited[i] && aDistance[i] < fCurrent)
            {
                nCurrent = (int)i;
                fCurrent = aDistance[i];
            }
        }

        if(nCurrent < 0 || nCurrent == nDest)
        {
            break;
        }

        aVisited[nCurrent] = true;

        for(uint32 i = 0; i < nCount; ++i)
        {
            if((int)i == nCurrent || aVisited[i])
            {
                continue;
            }

            LTVector vConnection;
            if(!FTGetConnection(aVolumes[nCurrent], aVolumes[i], nLane + nStep, &vConnection))
            {
                continue;
            }

            float fCost = (float)sqrt(
                aVolumes[nCurrent].vCenter.DistSqr(aVolumes[i].vCenter));

            if(aVolumes[i].pVolume->IsPreferredPath())
            {
                fCost *= 0.80f;
            }

            float fCandidate = aDistance[nCurrent] + fCost;
            if(fCandidate < aDistance[i])
            {
                aDistance[i] = fCandidate;
                aPrevious[i] = nCurrent;
            }
        }
    }

    if(aPrevious[nDest] < 0)
    {
        return false;
    }

    int aReverse[kMaxNavVolumes];
    uint32 nReverseCount = 0;
    int nWalk = nDest;

    while(nWalk >= 0 && nReverseCount < kMaxNavVolumes)
    {
        aReverse[nReverseCount++] = nWalk;
        if(nWalk == nSource)
        {
            break;
        }
        nWalk = aPrevious[nWalk];
    }

    if(nReverseCount == 0 || aReverse[nReverseCount - 1] != nSource)
    {
        return false;
    }

    for(int i = (int)nReverseCount - 1; i > 0; --i)
    {
        LTVector vConnection;
        if(FTGetConnection(
            aVolumes[aReverse[i]],
            aVolumes[aReverse[i - 1]],
            nLane + (uint32)i,
            &vConnection))
        {
            aWaypoints.push_back(vConnection);
        }
    }

    aWaypoints.push_back(vDestination);
    return true;
}

AIRegion::AIRegion()
{
    m_sName[0] = '\0';
    m_vDims.Init(0.0f, 0.0f, 0.0f);
}

void AIRegion::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;
    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(m_sName, prop.m_String, sizeof(m_sName) - 1);
        m_sName[sizeof(m_sName) - 1] = '\0';
        strncpy(pOCS->m_Name, m_sName, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    if(g_pLTServer->GetPropGeneric("Dims", &prop) == LT_OK)
    {
        m_vDims = prop.m_Vec;
    }

    pOCS->m_ObjectType = OT_NORMAL;
}

void AIRegion::PrintNavigationSummary()
{
    uint32 nVolumes = FT_GetNavigationVolumeCount();
    uint32 nPatrolNodes = 0;
    HCLASS hPatrolClass = g_pLTServer->GetClass("AINodePatrol");

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(hPatrolClass && hClass && g_pLTServer->IsKindOf(hClass, hPatrolClass))
        {
            ++nPatrolNodes;
        }
    }

    LTVector vPos;
    g_pLTServer->GetObjectPos(m_hObject, &vPos);
    g_pLTServer->CPrint(
        "Fireteam nav: region %s pos %.1f %.1f %.1f; %u AIVolumes, %u patrol nodes",
        m_sName, vPos.x, vPos.y, vPos.z, nVolumes, nPatrolNodes);
}

uint32 AIRegion::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS && fData == PRECREATE_WORLDFILE)
            {
                ReadProps(pOCS);
            }
        }
        break;

        case MID_INITIALUPDATE:
            g_pLTServer->SetNextUpdate(m_hObject, 1.0f);
            break;

        case MID_UPDATE:
            PrintNavigationSummary();
            g_pLTServer->SetNextUpdate(m_hObject, 0.0f);
            break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}

AINodePatrol::AINodePatrol() :
    m_bStartDisabled(false)
{
    m_sName[0] = '\0';
    m_sNext[0] = '\0';
}

void AINodePatrol::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;
    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(m_sName, prop.m_String, sizeof(m_sName) - 1);
        m_sName[sizeof(m_sName) - 1] = '\0';
        strncpy(pOCS->m_Name, m_sName, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    if(g_pLTServer->GetPropGeneric("Next", &prop) == LT_OK)
    {
        strncpy(m_sNext, prop.m_String, sizeof(m_sNext) - 1);
        m_sNext[sizeof(m_sNext) - 1] = '\0';
    }

    if(g_pLTServer->GetPropGeneric("StartDisabled", &prop) == LT_OK)
    {
        m_bStartDisabled = (prop.m_Bool != LTFALSE);
    }

    pOCS->m_ObjectType = OT_NORMAL;
}

uint32 AINodePatrol::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS && fData == PRECREATE_WORLDFILE)
            {
                ReadProps(pOCS);
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            LTVector vPos;
            g_pLTServer->GetObjectPos(m_hObject, &vPos);
            g_pLTServer->CPrint(
                "Fireteam nav node: %s -> %s at %.1f %.1f %.1f disabled=%u",
                m_sName,
                m_sNext[0] ? m_sNext : "<none>",
                vPos.x, vPos.y, vPos.z,
                m_bStartDisabled ? 1 : 0);
            g_pLTServer->SetNextUpdate(m_hObject, 0.0f);
        }
        break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}
'@

$zombieH = @'
#ifndef __FIRETEAM_ZOMBIE_H__
#define __FIRETEAM_ZOMBIE_H__

#include <ltengineobjects.h>
#include <vector>

class FireteamZombie : public BaseClass
{
public:
    FireteamZombie();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);
    uint32 ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg);

private:
    HOBJECT FindNearestPlayer();
    void UpdateZombie();
    void RebuildPath(const LTVector &vTarget);

    uint16 m_nHealth;
    float m_fAttackCooldown;
    float m_fRepathCooldown;
    float m_fStuckTime;
    uint32 m_nPathLane;
    uint32 m_nWaypoint;
    LTVector m_vLastPos;
    std::vector<LTVector> m_aPath;
};

#endif
'@

$zombieCpp = @'
#include "FireteamZombie.h"
#include "FireteamNavigation.h"
#include "FireteamSpawner.h"
#include "playersrvr.h"
#include "serverinterfaces.h"
#include "msgids.h"

#include <iltcommon.h>
#include <iltmodel.h>
#include <iltphysics.h>
#include <ltobjectcreate.h>
#include <float.h>
#include <string.h>

BEGIN_CLASS(FireteamZombie)
END_CLASS_DEFAULT_FLAGS(FireteamZombie, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

static uint32 s_nZombieSerial = 0;

FireteamZombie::FireteamZombie() :
    m_nHealth(40),
    m_fAttackCooldown(0.0f),
    m_fRepathCooldown(0.0f),
    m_fStuckTime(0.0f),
    m_nPathLane((s_nZombieSerial++) % 3),
    m_nWaypoint(0)
{
    m_vLastPos.Init(0.0f, 0.0f, 0.0f);
}

HOBJECT FireteamZombie::FindNearestPlayer()
{
    HCLASS hPlayerClass = g_pLTServer->GetClass("CPlayerSrvr");
    if(!hPlayerClass)
    {
        return LTNULL;
    }

    LTVector vMyPos;
    g_pLTServer->GetObjectPos(m_hObject, &vMyPos);

    HOBJECT hBest = LTNULL;
    float fBestDist = FLT_MAX;

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(!hClass || !g_pLTServer->IsKindOf(hClass, hPlayerClass))
        {
            continue;
        }

        CPlayerSrvr *pPlayer = (CPlayerSrvr*)g_pLTServer->HandleToObject(hObj);
        if(!pPlayer || !pPlayer->IsAlive())
        {
            continue;
        }

        LTVector vPlayerPos;
        g_pLTServer->GetObjectPos(hObj, &vPlayerPos);
        float fDist = vMyPos.DistSqr(vPlayerPos);

        if(fDist < fBestDist)
        {
            fBestDist = fDist;
            hBest = hObj;
        }
    }

    return hBest;
}

void FireteamZombie::RebuildPath(const LTVector &vTarget)
{
    LTVector vPos;
    g_pLTServer->GetObjectPos(m_hObject, &vPos);

    m_aPath.clear();
    if(!FT_BuildNavigationPath(vPos, vTarget, m_nPathLane, m_aPath))
    {
        m_aPath.push_back(vTarget);
    }

    m_nWaypoint = 0;
    m_fRepathCooldown = 0.75f + ((float)m_nPathLane * 0.15f);
}

void FireteamZombie::UpdateZombie()
{
    const float kUpdate = 0.10f;
    const float kMoveSpeed = 95.0f;
    const float kAttackRange = 55.0f;
    const float kWaypointRadius = 30.0f;
    const float kSeparationRadius = 58.0f;

    if(m_fAttackCooldown > 0.0f) m_fAttackCooldown -= kUpdate;
    if(m_fRepathCooldown > 0.0f) m_fRepathCooldown -= kUpdate;

    HOBJECT hTarget = FindNearestPlayer();
    if(!hTarget)
    {
        LTVector vStop(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vStop);
        return;
    }

    LTVector vPos;
    LTVector vTarget;
    g_pLTServer->GetObjectPos(m_hObject, &vPos);
    g_pLTServer->GetObjectPos(hTarget, &vTarget);

    LTVector vToPlayer = vTarget - vPos;
    vToPlayer.y = 0.0f;
    float fPlayerDistance = vToPlayer.Mag();

    if(fPlayerDistance <= kAttackRange)
    {
        LTVector vStop(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vStop);

        if(fPlayerDistance > 1.0f)
        {
            vToPlayer.Normalize();
            LTRotation rLook(vToPlayer, LTVector(0.0f, 1.0f, 0.0f));
            g_pLTServer->SetObjectRotation(m_hObject, &rLook);
        }

        if(m_fAttackCooldown <= 0.0f)
        {
            CPlayerSrvr *pPlayer = (CPlayerSrvr*)g_pLTServer->HandleToObject(hTarget);
            if(pPlayer)
            {
                pPlayer->ApplyDamage(10);
            }
            m_fAttackCooldown = 1.0f;
        }

        m_fStuckTime = 0.0f;
        m_vLastPos = vPos;
        return;
    }

    if(m_fRepathCooldown <= 0.0f || m_aPath.empty() || m_nWaypoint >= m_aPath.size())
    {
        RebuildPath(vTarget);
    }

    while(m_nWaypoint < m_aPath.size())
    {
        LTVector vCheck = m_aPath[m_nWaypoint] - vPos;
        vCheck.y = 0.0f;
        if(vCheck.Mag() > kWaypointRadius)
        {
            break;
        }
        ++m_nWaypoint;
    }

    if(m_nWaypoint >= m_aPath.size())
    {
        RebuildPath(vTarget);
    }

    LTVector vMoveTarget = (m_nWaypoint < m_aPath.size()) ? m_aPath[m_nWaypoint] : vTarget;
    LTVector vMove = vMoveTarget - vPos;
    vMove.y = 0.0f;

    LTVector vSeparation(0.0f, 0.0f, 0.0f);
    HCLASS hZombieClass = g_pLTServer->GetClass("FireteamZombie");

    if(hZombieClass)
    {
        for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
            hObj;
            hObj = g_pLTServer->GetNextObject(hObj))
        {
            if(hObj == m_hObject) continue;

            HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
            if(!hClass || !g_pLTServer->IsKindOf(hClass, hZombieClass)) continue;

            LTVector vOther;
            g_pLTServer->GetObjectPos(hObj, &vOther);

            LTVector vAway = vPos - vOther;
            vAway.y = 0.0f;
            float fDistance = vAway.Mag();

            if(fDistance > 0.5f && fDistance < kSeparationRadius)
            {
                vAway.Normalize();
                float fStrength = (kSeparationRadius - fDistance) / kSeparationRadius;
                vSeparation += vAway * fStrength;
            }
        }
    }

    if(vMove.Mag() > 1.0f)
    {
        vMove.Normalize();
        LTVector vSteer = vMove + (vSeparation * 0.85f);

        if(vSteer.Mag() > 0.1f) vSteer.Normalize();
        else vSteer = vMove;

        LTRotation rLook(vSteer, LTVector(0.0f, 1.0f, 0.0f));
        g_pLTServer->SetObjectRotation(m_hObject, &rLook);

        LTVector vVelocity = vSteer * kMoveSpeed;
        vVelocity.y = -80.0f;
        g_pLTSPhysics->SetVelocity(m_hObject, &vVelocity);
    }

    LTVector vMoved = vPos - m_vLastPos;
    vMoved.y = 0.0f;

    if(vMoved.Mag() < 1.0f)
    {
        m_fStuckTime += kUpdate;
        if(m_fStuckTime >= 0.80f)
        {
            m_nPathLane = (m_nPathLane + 1) % 3;
            m_fRepathCooldown = 0.0f;
            m_fStuckTime = 0.0f;
        }
    }
    else
    {
        m_fStuckTime = 0.0f;
    }

    m_vLastPos = vPos;
}

uint32 FireteamZombie::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS)
            {
                pOCS->m_ObjectType = OT_MODEL;
                pOCS->m_Flags |= FLAG_SOLID | FLAG_VISIBLE | FLAG_GRAVITY |
                                 FLAG_YROTATION | FLAG_FORCECLIENTUPDATE | FLAG_SHADOW;
                pOCS->m_Flags2 |= FLAG2_PLAYERCOLLIDE;

                strncpy(pOCS->m_Filenames[0], "Models\\HARMGuard.ltb", MAX_CS_FILENAME_LEN - 1);
                strncpy(pOCS->m_Filenames[1], "Models\\playerbase.ltb", MAX_CS_FILENAME_LEN - 1);
                strncpy(pOCS->m_SkinNames[0], "ModelTextures\\HARMPurple.dtx", MAX_CS_FILENAME_LEN - 1);
                strncpy(pOCS->m_SkinNames[1], "ModelTextures\\HARMHeadW1.dtx", MAX_CS_FILENAME_LEN - 1);
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            g_pLTServer->SetObjectColor(m_hObject, 0.45f, 0.90f, 0.45f, 1.0f);

            HMODELANIM hAnim = g_pLTServer->GetAnimIndex(m_hObject, "LRF");
            if(hAnim != INVALID_MODEL_ANIM)
            {
                g_pLTSModel->SetCurAnim(m_hObject, MAIN_TRACKER, hAnim);
                g_pLTSModel->SetLooping(m_hObject, MAIN_TRACKER, LTTRUE);

                LTVector vDims;
                if(g_pLTSCommon->GetModelAnimUserDims(m_hObject, &vDims, hAnim) == LT_OK)
                {
                    g_pLTSPhysics->SetObjectDims(m_hObject, &vDims, 0);
                }
            }

            g_pLTServer->GetObjectPos(m_hObject, &m_vLastPos);
            g_pLTServer->SetNextUpdate(m_hObject, 0.10f);
        }
        break;

        case MID_UPDATE:
            UpdateZombie();
            g_pLTServer->SetNextUpdate(m_hObject, 0.10f);
            break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}

uint32 FireteamZombie::ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg)
{
    pMsg->SeekTo(0);
    uint32 messageID = pMsg->Readuint32();

    if(messageID == OBJ_MID_DAMAGE)
    {
        uint8 nDamage = pMsg->Readuint8();
        m_nHealth = (nDamage >= m_nHealth) ? 0 : (uint16)(m_nHealth - nDamage);

        if(m_nHealth == 0)
        {
            FT_OnFireteamEnemyKilled();
            g_pLTServer->CPrint("Fireteam: placeholder zombie killed.");
            g_pLTServer->RemoveObject(m_hObject);
            return 1;
        }
    }

    return BaseClass::ObjectMessageFn(hSender, pMsg);
}
'@

$spawnerH = @'
#ifndef __FIRETEAM_SPAWNER_H__
#define __FIRETEAM_SPAWNER_H__

#include <ltengineobjects.h>

class Spawner : public BaseClass
{
public:
    Spawner();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);
    bool IsPerimeterSpawner() const;
    bool SpawnZombie();
    bool SpawnCrawlerSeal();
    void ResetRoundController();
    void CollectPerimeterSpawners();
    void StartNextRound();
    void UpdateRoundController();

    char m_sName[64];
};

void FT_OnFireteamEnemyKilled();

#endif
'@

$spawnerCpp = @'
#include "FireteamSpawner.h"
#include "serverinterfaces.h"

#include <ltobjectcreate.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

BEGIN_CLASS(Spawner)
    ADD_STRINGPROP(DefaultSpawn, "")
    ADD_STRINGPROP(Target, "")
    ADD_STRINGPROP(SpawnSound, "")
    ADD_REALPROP(SoundRadius, 500.0f)
    ADD_STRINGPROP(InitialCommand, "")
END_CLASS_DEFAULT_FLAGS(Spawner, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

static const uint32 kMaxPerimeterSpawners = 32;
static const uint32 kSealCrawlerChancePercent = 2;

static HOBJECT s_hController = LTNULL;
static HOBJECT s_hPerimeterSpawners[kMaxPerimeterSpawners];
static uint32 s_nPerimeterSpawnerCount = 0;
static int s_nLastSpawner = -1;

static bool s_bRoundActive = false;
static bool s_bRoundIntermission = false;
static uint32 s_nRound = 0;
static uint32 s_nRoundTarget = 0;
static uint32 s_nRoundSpawned = 0;
static uint32 s_nRoundAlive = 0;
static uint32 s_nRoundKilled = 0;
static uint32 s_nMaxAlive = 0;
static float s_fNextSpawnTime = 0.0f;
static float s_fNextRoundTime = 0.0f;
static float s_fSpawnInterval = 1.25f;

Spawner::Spawner()
{
    m_sName[0] = '\0';
}

void Spawner::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;
    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(m_sName, prop.m_String, sizeof(m_sName) - 1);
        m_sName[sizeof(m_sName) - 1] = '\0';
        strncpy(pOCS->m_Name, prop.m_String, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    pOCS->m_ObjectType = OT_NORMAL;
}

bool Spawner::IsPerimeterSpawner() const
{
    return (_strnicmp(m_sName, "Spawner_01_", 11) == 0);
}

void Spawner::ResetRoundController()
{
    s_hController = m_hObject;
    s_nPerimeterSpawnerCount = 0;
    s_nLastSpawner = -1;

    for(uint32 i = 0; i < kMaxPerimeterSpawners; ++i)
    {
        s_hPerimeterSpawners[i] = LTNULL;
    }

    s_bRoundActive = false;
    s_bRoundIntermission = false;
    s_nRound = 0;
    s_nRoundTarget = 0;
    s_nRoundSpawned = 0;
    s_nRoundAlive = 0;
    s_nRoundKilled = 0;
    s_nMaxAlive = 0;
    s_fNextSpawnTime = 0.0f;
    s_fNextRoundTime = 0.0f;
    s_fSpawnInterval = 1.25f;

    srand((unsigned int)time(LTNULL));
}

void Spawner::CollectPerimeterSpawners()
{
    s_nPerimeterSpawnerCount = 0;

    HCLASS hSpawnerClass = g_pLTServer->GetClass("Spawner");
    if(!hSpawnerClass)
    {
        return;
    }

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj && s_nPerimeterSpawnerCount < kMaxPerimeterSpawners;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(!hClass || !g_pLTServer->IsKindOf(hClass, hSpawnerClass))
        {
            continue;
        }

        Spawner *pSpawner = (Spawner*)g_pLTServer->HandleToObject(hObj);
        if(pSpawner && pSpawner->IsPerimeterSpawner())
        {
            s_hPerimeterSpawners[s_nPerimeterSpawnerCount++] = hObj;
        }
    }

    g_pLTServer->CPrint(
        "Fireteam: registered %u Cabin Fever perimeter spawners.",
        s_nPerimeterSpawnerCount);
}

bool Spawner::SpawnZombie()
{
    HCLASS hZombieClass = g_pLTServer->GetClass("FireteamZombie");
    if(!hZombieClass)
    {
        return false;
    }

    LTVector vBasePos;
    LTRotation rBaseRot;
    g_pLTServer->GetObjectPos(m_hObject, &vBasePos);
    g_pLTServer->GetObjectRotation(m_hObject, &rBaseRot);

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Pos = vBasePos;
    ocs.m_Pos.x += (float)((rand() % 29) - 14);
    ocs.m_Pos.z += (float)((rand() % 29) - 14);
    ocs.m_Pos.y += 100.0f;
    ocs.m_Rotation = rBaseRot;

    return g_pLTServer->CreateObject(hZombieClass, &ocs) != LTNULL;
}

bool Spawner::SpawnCrawlerSeal()
{
    HCLASS hSealClass = g_pLTServer->GetClass("Seal");
    if(!hSealClass)
    {
        return false;
    }

    LTVector vBasePos;
    LTRotation rBaseRot;
    g_pLTServer->GetObjectPos(m_hObject, &vBasePos);
    g_pLTServer->GetObjectRotation(m_hObject, &rBaseRot);

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    strcpy(ocs.m_Filename, "Models/seal.ltb");
    strcpy(ocs.m_SkinName, "ModelTextures/seal.dtx");

    ocs.m_Pos = vBasePos;
    ocs.m_Pos.x += (float)((rand() % 41) - 20);
    ocs.m_Pos.z += (float)((rand() % 41) - 20);
    ocs.m_Pos.y += 80.0f;
    ocs.m_Rotation = rBaseRot;

    if(g_pLTServer->CreateObject(hSealClass, &ocs))
    {
        g_pLTServer->CPrint(
            "Fireteam: crawler seal easter egg spawned from %s.",
            m_sName);
        return true;
    }

    return false;
}

void Spawner::StartNextRound()
{
    ++s_nRound;

    s_nRoundTarget = 6 + ((s_nRound - 1) * 2);
    if(s_nRoundTarget > 30) s_nRoundTarget = 30;

    s_nMaxAlive = 3 + ((s_nRound - 1) / 2);
    if(s_nMaxAlive > 8) s_nMaxAlive = 8;

    s_fSpawnInterval = 1.25f - ((float)(s_nRound - 1) * 0.05f);
    if(s_fSpawnInterval < 0.60f) s_fSpawnInterval = 0.60f;

    s_nRoundSpawned = 0;
    s_nRoundAlive = 0;
    s_nRoundKilled = 0;
    s_bRoundActive = true;
    s_bRoundIntermission = false;
    s_fNextSpawnTime = g_pLTServer->GetTime() + 0.35f;

    g_pLTServer->CPrint(
        "Fireteam: DEV ROUND %u START - %u infected, max %u alive.",
        s_nRound, s_nRoundTarget, s_nMaxAlive);
}

void Spawner::UpdateRoundController()
{
    if(s_hController != m_hObject)
    {
        return;
    }

    if(s_nPerimeterSpawnerCount == 0)
    {
        CollectPerimeterSpawners();
        if(s_nPerimeterSpawnerCount == 0) return;
    }

    float fNow = g_pLTServer->GetTime();

    if(!s_bRoundActive && !s_bRoundIntermission)
    {
        StartNextRound();
    }

    if(s_bRoundIntermission)
    {
        if(fNow >= s_fNextRoundTime) StartNextRound();
        return;
    }

    if(!s_bRoundActive ||
       s_nRoundSpawned >= s_nRoundTarget ||
       s_nRoundAlive >= s_nMaxAlive ||
       fNow < s_fNextSpawnTime)
    {
        return;
    }

    int nChoice = 0;
    if(s_nPerimeterSpawnerCount > 1)
    {
        for(uint32 nTry = 0; nTry < 8; ++nTry)
        {
            nChoice = rand() % s_nPerimeterSpawnerCount;
            if(nChoice != s_nLastSpawner) break;
        }
    }

    HOBJECT hSpawnObject = s_hPerimeterSpawners[nChoice];
    Spawner *pSpawn = hSpawnObject ?
        (Spawner*)g_pLTServer->HandleToObject(hSpawnObject) :
        LTNULL;

    if(pSpawn && pSpawn->SpawnZombie())
    {
        ++s_nRoundSpawned;
        ++s_nRoundAlive;
        s_nLastSpawner = nChoice;

        g_pLTServer->CPrint(
            "Fireteam: round %u spawn %u/%u from %s (%u alive).",
            s_nRound,
            s_nRoundSpawned,
            s_nRoundTarget,
            pSpawn->m_sName,
            s_nRoundAlive);

        if((rand() % 100) < (int)kSealCrawlerChancePercent)
        {
            pSpawn->SpawnCrawlerSeal();
        }
    }

    float fJitter = ((float)(rand() % 51) / 100.0f);
    s_fNextSpawnTime = fNow + s_fSpawnInterval + fJitter;
}

void FT_OnFireteamEnemyKilled()
{
    if(s_nRoundAlive > 0) --s_nRoundAlive;

    if(s_bRoundActive)
    {
        ++s_nRoundKilled;

        if(s_nRoundKilled >= s_nRoundTarget &&
           s_nRoundSpawned >= s_nRoundTarget)
        {
            s_bRoundActive = false;
            s_bRoundIntermission = true;
            s_fNextRoundTime = g_pLTServer->GetTime() + 5.0f;

            g_pLTServer->CPrint(
                "Fireteam: DEV ROUND %u CLEAR - next round in 5 seconds.",
                s_nRound);
        }
    }
}

uint32 Spawner::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS && fData == PRECREATE_WORLDFILE)
            {
                ReadProps(pOCS);
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            if(_stricmp(m_sName, "Spawner_01_01N") == 0)
            {
                ResetRoundController();
                g_pLTServer->SetNextUpdate(m_hObject, 1.25f);
            }
            else
            {
                g_pLTServer->SetNextUpdate(m_hObject, 0.0f);
            }
        }
        break;

        case MID_UPDATE:
            UpdateRoundController();
            g_pLTServer->SetNextUpdate(m_hObject, 0.20f);
            break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}
'@

Write-Source (Join-Path $sealRoot 'sshell\src\FireteamNavigation.h') $navigationH
Write-Source (Join-Path $sealRoot 'sshell\src\FireteamNavigation.cpp') $navigationCpp
Write-Source (Join-Path $sealRoot 'sshell\src\FireteamZombie.h') $zombieH
Write-Source (Join-Path $sealRoot 'sshell\src\FireteamZombie.cpp') $zombieCpp
Write-Source (Join-Path $sealRoot 'sshell\src\FireteamSpawner.h') $spawnerH
Write-Source (Join-Path $sealRoot 'sshell\src\FireteamSpawner.cpp') $spawnerCpp

Write-Host "[OK] Fireteam routed AI, staggered perimeter rounds, rare crawler seal"
