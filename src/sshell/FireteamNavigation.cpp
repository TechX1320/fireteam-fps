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
static float FTClamp(float v, float lo, float hi)
{
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static float FTChooseGateCoordinate(
    float fReference,
    float fLo,
    float fHi,
    float fAgentHalfWidth,
    uint32 nLane)
{
    if(fLo >= fHi)
    {
        return (fLo + fHi) * 0.5f;
    }

    const float fGateWidth =
        FTMax(
            12.0f,
            fAgentHalfWidth * 2.0f);
    const float fSpan =
        fHi - fLo;

    uint32 nGateCount =
        (uint32)(fSpan / fGateWidth) + 1;

    if(nGateCount <= 1)
    {
        return (fLo + fHi) * 0.5f;
    }

    const float fSpacing =
        fSpan /
        (float)(nGateCount - 1);

    const float fClampedReference =
        FTClamp(
            fReference,
            fLo,
            fHi);

    int nNearest =
        (int)(((fClampedReference - fLo) /
               fSpacing) + 0.5f);

    // NOLF2 divides volume connections into character-width gates and
    // allocates a nearby gate. FIRETEAM does not keep shared occupancy yet,
    // so use a stable per-infected lane and rotate it during stuck recovery.
    const uint32 nPatternCount =
        (nGateCount * 2) - 1;
    const uint32 nPattern =
        nLane % nPatternCount;

    int nOffset = 0;
    if(nPattern > 0)
    {
        const int nStep =
            (int)((nPattern + 1) / 2);

        nOffset =
            (nPattern & 1)
            ? nStep
            : -nStep;
    }

    int nSelected =
        nNearest + nOffset;

    if(nSelected < 0 ||
       nSelected >= (int)nGateCount)
    {
        nSelected =
            nNearest - nOffset;
    }

    if(nSelected < 0 ||
       nSelected >= (int)nGateCount)
    {
        nSelected = nNearest;
    }

    return fLo +
        (fSpacing * (float)nSelected);
}

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

static int FTFindContainingVolume(
    const FTNavVolume *pVolumes,
    uint32 nCount,
    const LTVector &vPos)
{
    int nBest = -1;
    float fBestVerticalOutside = FLT_MAX;
    float fBestVerticalCenter = FLT_MAX;
    float fBestArea = FLT_MAX;

    for(uint32 i = 0; i < nCount; ++i)
    {
        const LTVector &c = pVolumes[i].vCenter;
        const LTVector &d = pVolumes[i].vDims;

        const float dx =
            (float)fabs(vPos.x - c.x) - d.x;
        const float dz =
            (float)fabs(vPos.z - c.z) - d.z;
        const float fVerticalCenter =
            (float)fabs(vPos.y - c.y);
        const float fVerticalOutside =
            FTMax(
                0.0f,
                fVerticalCenter - d.y);

        // Imported Cabin Fever volumes can overlap in X/Z on separate floors.
        // Keep the old 96-unit vertical tolerance for actor origins that sit
        // above a thin authored volume, but never let a smaller upstairs
        // footprint beat the volume that is actually closest to this floor.
        if(dx <= 6.0f &&
           dz <= 6.0f &&
           fVerticalOutside <= 96.0f)
        {
            const float fArea =
                FTMax(
                    1.0f,
                    d.x * d.z);

            const bool bBetterOutside =
                fVerticalOutside <
                    (fBestVerticalOutside - 0.01f);
            const bool bSameOutside =
                (float)fabs(
                    fVerticalOutside -
                    fBestVerticalOutside) <= 0.01f;
            const bool bBetterCenter =
                fVerticalCenter <
                    (fBestVerticalCenter - 0.01f);
            const bool bSameCenter =
                (float)fabs(
                    fVerticalCenter -
                    fBestVerticalCenter) <= 0.01f;

            if(bBetterOutside ||
               (bSameOutside &&
                (bBetterCenter ||
                 (bSameCenter &&
                  fArea < fBestArea))))
            {
                fBestVerticalOutside =
                    fVerticalOutside;
                fBestVerticalCenter =
                    fVerticalCenter;
                fBestArea = fArea;
                nBest = (int)i;
            }
        }
    }

    return nBest;
}

static int FTFindVolume(
    const FTNavVolume *pVolumes,
    uint32 nCount,
    const LTVector &vPos)
{
    const int nContaining =
        FTFindContainingVolume(
            pVolumes,
            nCount,
            vPos);

    if(nContaining >= 0)
    {
        return nContaining;
    }

    int nBest = -1;
    float fBest = FLT_MAX;

    for(uint32 i = 0; i < nCount; ++i)
    {
        const LTVector &c = pVolumes[i].vCenter;
        const LTVector &d = pVolumes[i].vDims;

        const float dx =
            FTMax(
                0.0f,
                (float)fabs(vPos.x - c.x) -
                d.x);
        const float dz =
            FTMax(
                0.0f,
                (float)fabs(vPos.z - c.z) -
                d.z);
        const float dy =
            FTMax(
                0.0f,
                (float)fabs(vPos.y - c.y) -
                (d.y + 96.0f));

        const float fScore =
            (dx * dx) +
            (dz * dz) +
            (dy * dy * 0.25f);

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
    const LTVector &vReference,
    uint32 nLane,
    LTVector *pOut)
{
    const float kSlack = 18.0f;
    const float kVerticalSlack = 96.0f;

    // NOLF2 chooses the opening nearest the current control point instead of
    // rotating through arbitrary lane numbers. That keeps an AI committed to
    // a sensible corridor and prevents the path from zig-zagging between
    // opposite edges of adjacent AIVolumes.
    float fHalfWidth = fAgentHalfWidth * 1.25f;
    if(fHalfWidth < 6.0f) fHalfWidth = 6.0f;
    if(fHalfWidth > 42.0f) fHalfWidth = 42.0f;

    const float kMinOpening = fHalfWidth * 2.0f;

    const float aMinX = a.vCenter.x - a.vDims.x;
    const float aMaxX = a.vCenter.x + a.vDims.x;
    const float aMinY = a.vCenter.y - a.vDims.y;
    const float aMaxY = a.vCenter.y + a.vDims.y;
    const float aMinZ = a.vCenter.z - a.vDims.z;
    const float aMaxZ = a.vCenter.z + a.vDims.z;

    const float bMinX = b.vCenter.x - b.vDims.x;
    const float bMaxX = b.vCenter.x + b.vDims.x;
    const float bMinY = b.vCenter.y - b.vDims.y;
    const float bMaxY = b.vCenter.y + b.vDims.y;
    const float bMinZ = b.vCenter.z - b.vDims.z;
    const float bMaxZ = b.vCenter.z + b.vDims.z;

    const float overlapXMin = FTMax(aMinX, bMinX);
    const float overlapXMax = FTMin(aMaxX, bMaxX);
    const float overlapZMin = FTMax(aMinZ, bMinZ);
    const float overlapZMax = FTMin(aMaxZ, bMaxZ);
    const float overlapX = overlapXMax - overlapXMin;
    const float overlapZ = overlapZMax - overlapZMin;

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

    if(gapX <= kSlack && overlapZ >= kMinOpening)
    {
        if(overlapX >= 0.0f)
            pOut->x = (overlapXMin + overlapXMax) * 0.5f;
        else if(a.vCenter.x < b.vCenter.x)
            pOut->x = (aMaxX + bMinX) * 0.5f;
        else
            pOut->x = (bMaxX + aMinX) * 0.5f;

        const float fLo = overlapZMin + fHalfWidth;
        const float fHi = overlapZMax - fHalfWidth;
        pOut->z = (fLo <= fHi)
            ? FTChooseGateCoordinate(
                vReference.z,
                fLo,
                fHi,
                fHalfWidth,
                nLane)
            : (overlapZMin + overlapZMax) * 0.5f;

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

        const float fLo = overlapXMin + fHalfWidth;
        const float fHi = overlapXMax - fHalfWidth;
        pOut->x = (fLo <= fHi)
            ? FTChooseGateCoordinate(
                vReference.x,
                fLo,
                fHi,
                fHalfWidth,
                nLane)
            : (overlapXMin + overlapXMax) * 0.5f;

        pOut->y = (a.vCenter.y + b.vCenter.y) * 0.5f;
        return true;
    }

    if(overlapX >= kMinOpening && overlapZ >= kMinOpening)
    {
        // Fully overlapping volumes. Choose the coordinate nearest the
        // approach point on the non-crossing axis, just like NOLF2's
        // FindNearestEntryPoint behavior.
        const float dxCenter =
            (float)fabs(a.vCenter.x - b.vCenter.x);
        const float dzCenter =
            (float)fabs(a.vCenter.z - b.vCenter.z);

        if(dxCenter >= dzCenter)
        {
            pOut->x = (overlapXMin + overlapXMax) * 0.5f;
            pOut->z = FTChooseGateCoordinate(
                vReference.z,
                overlapZMin + fHalfWidth,
                overlapZMax - fHalfWidth,
                fHalfWidth,
                nLane);
        }
        else
        {
            pOut->x = FTChooseGateCoordinate(
                vReference.x,
                overlapXMin + fHalfWidth,
                overlapXMax - fHalfWidth,
                fHalfWidth,
                nLane);
            pOut->z = (overlapZMin + overlapZMax) * 0.5f;
        }

        pOut->y = (a.vCenter.y + b.vCenter.y) * 0.5f;
        return true;
    }

    return false;
}

bool FT_ArePositionsInSameNavigationVolume(
    const LTVector &vA,
    const LTVector &vB)
{
    FTNavVolume aVolumes[kMaxNavVolumes];
    const uint32 nCount =
        FTCollectVolumes(
            aVolumes,
            kMaxNavVolumes);

    if(nCount == 0)
    {
        return false;
    }

    const int nA =
        FTFindContainingVolume(
            aVolumes,
            nCount,
            vA);
    const int nB =
        FTFindContainingVolume(
            aVolumes,
            nCount,
            vB);

    return nA >= 0 &&
           nB >= 0 &&
           nA == nB;
}

bool FT_IsPositionInNavigationVolume(
    const LTVector &vPos)
{
    FTNavVolume aVolumes[kMaxNavVolumes];
    const uint32 nCount =
        FTCollectVolumes(
            aVolumes,
            kMaxNavVolumes);

    return nCount > 0 &&
           FTFindContainingVolume(
               aVolumes,
               nCount,
               vPos) >= 0;
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
            if(!FTGetConnection(
                aVolumes[nCurrent],
                aVolumes[i],
                fAgentHalfWidth,
                aVolumes[nCurrent].vCenter,
                0,
                &vConnection))
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

    LTVector vReference = vStart;

    for(int i = (int)nReverseCount - 1; i > 0; --i)
    {
        const FTNavVolume &from =
            aVolumes[aReverse[i]];
        const FTNavVolume &to =
            aVolumes[aReverse[i - 1]];

        // For the final crossing, bias the gate toward the actual player
        // destination. Intermediate crossings stay near the preceding control
        // point, matching the NOLF2 path builder's nearest-entry strategy.
        const LTVector vGateReference =
            (i == 1)
            ? vDestination
            : vReference;

        LTVector vConnection;
        if(FTGetConnection(
            from,
            to,
            fAgentHalfWidth,
            vGateReference,
            nLane,
            &vConnection))
        {
            aWaypoints.push_back(vConnection);

            // Move the next control point just inside the destination volume.
            // A point exactly on a shared boundary is easy for box collision
            // to wedge against a jamb, especially in Cabin Fever doorways.
            LTVector vCross = to.vCenter - from.vCenter;
            vCross.y = 0.0f;

            if(vCross.MagSqr() > 1.0f)
            {
                vCross.Normalize();
                LTVector vInside =
                    vConnection +
                    (vCross * (fAgentHalfWidth + 4.0f));

                const float fMargin =
                    FTMax(4.0f, fAgentHalfWidth);

                vInside.x = FTClamp(
                    vInside.x,
                    to.vCenter.x - to.vDims.x + fMargin,
                    to.vCenter.x + to.vDims.x - fMargin);
                vInside.z = FTClamp(
                    vInside.z,
                    to.vCenter.z - to.vDims.z + fMargin,
                    to.vCenter.z + to.vDims.z - fMargin);

                aWaypoints.push_back(vInside);
                vReference = vInside;
            }
            else
            {
                vReference = vConnection;
            }
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