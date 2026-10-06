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
    float fAgentHalfWidth,
    uint32 nLane,
    LTVector *pOut)
{
    const float kSlack = 18.0f;
    const float kVerticalSlack = 96.0f;

    // The old prototype assumed NOLF2's 24-unit human half-width everywhere,
    // which made a 48-unit doorway mathematically have zero usable clearance.
    // Use the actual infected model width instead.
    float fHalfWidth = fAgentHalfWidth;
    if(fHalfWidth < 6.0f) fHalfWidth = 6.0f;
    if(fHalfWidth > 40.0f) fHalfWidth = 40.0f;

    const float kMinOpening = fHalfWidth * 2.0f;
    const float kGateStride = (fHalfWidth * 2.0f) + 8.0f;

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

    // NOLF2's AIVolumeNeighbor divides a connection into 48-unit gates and
    // places each gate at 24 + 48*n along the shared opening. This guarantees
    // enough clearance for its default 24-unit human half-width.
    if(gapX <= kSlack && overlapZ >= kMinOpening)
    {
        const float fUsable = overlapZ - (fHalfWidth * 2.0f);
        uint32 nGates = 1;
        if(fUsable > kGateStride)
        {
            nGates += (uint32)(fUsable / kGateStride);
        }
        uint32 nGate = nLane % nGates;

        if(overlapX >= 0.0f)
            pOut->x = (overlapXMin + overlapXMax) * 0.5f;
        else if(a.vCenter.x < b.vCenter.x)
            pOut->x = (aMaxX + bMinX) * 0.5f;
        else
            pOut->x = (bMaxX + aMinX) * 0.5f;

        if(nGates == 1)
        {
            pOut->z = (overlapZMin + overlapZMax) * 0.5f;
        }
        else
        {
            const float fStep = fUsable / (float)(nGates - 1);
            pOut->z = overlapZMin + fHalfWidth + (fStep * (float)nGate);
        }

        pOut->y = (a.vCenter.y + b.vCenter.y) * 0.5f;
        return true;
    }

    if(gapZ <= kSlack && overlapX >= kMinOpening)
    {
        const float fUsable = overlapX - (fHalfWidth * 2.0f);
        uint32 nGates = 1;
        if(fUsable > kGateStride)
        {
            nGates += (uint32)(fUsable / kGateStride);
        }
        uint32 nGate = nLane % nGates;

        if(overlapZ >= 0.0f)
            pOut->z = (overlapZMin + overlapZMax) * 0.5f;
        else if(a.vCenter.z < b.vCenter.z)
            pOut->z = (aMaxZ + bMinZ) * 0.5f;
        else
            pOut->z = (bMaxZ + aMinZ) * 0.5f;

        if(nGates == 1)
        {
            pOut->x = (overlapXMin + overlapXMax) * 0.5f;
        }
        else
        {
            const float fStep = fUsable / (float)(nGates - 1);
            pOut->x = overlapXMin + fHalfWidth + (fStep * (float)nGate);
        }

        pOut->y = (a.vCenter.y + b.vCenter.y) * 0.5f;
        return true;
    }

    if(overlapX >= kMinOpening && overlapZ >= kMinOpening)
    {
        // Fully overlapping volumes: treat the dominant center separation as
        // the crossing direction and allocate gates on the other axis.
        float dxCenter = (float)fabs(a.vCenter.x - b.vCenter.x);
        float dzCenter = (float)fabs(a.vCenter.z - b.vCenter.z);

        if(dxCenter >= dzCenter)
        {
            const float fUsable = overlapZ - (fHalfWidth * 2.0f);
            uint32 nGates = 1;
            if(fUsable > kGateStride)
                nGates += (uint32)(fUsable / kGateStride);
            uint32 nGate = nLane % nGates;

            pOut->x = (overlapXMin + overlapXMax) * 0.5f;
            pOut->z =
                (nGates == 1)
                ? (overlapZMin + overlapZMax) * 0.5f
                : overlapZMin + fHalfWidth +
                    ((fUsable / (float)(nGates - 1)) * (float)nGate);
        }
        else
        {
            const float fUsable = overlapX - (fHalfWidth * 2.0f);
            uint32 nGates = 1;
            if(fUsable > kGateStride)
                nGates += (uint32)(fUsable / kGateStride);
            uint32 nGate = nLane % nGates;

            pOut->x =
                (nGates == 1)
                ? (overlapXMin + overlapXMax) * 0.5f
                : overlapXMin + fHalfWidth +
                    ((fUsable / (float)(nGates - 1)) * (float)nGate);
            pOut->z = (overlapZMin + overlapZMax) * 0.5f;
        }

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
    float fAgentHalfWidth,
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
            if(!FTGetConnection(aVolumes[nCurrent], aVolumes[i], fAgentHalfWidth, nLane + nStep, &vConnection))
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
            fAgentHalfWidth,
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