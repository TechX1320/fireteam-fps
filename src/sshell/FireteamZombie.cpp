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

        // NOLF2's cheap human movement advances the desired X/Z position,
        // finds the floor beneath it, then uses MoveObject. This handles stairs
        // and small height changes much better than forcing a horizontal velocity.
        LTVector vDesired = vPos + (vSteer * (kMoveSpeed * kUpdate));

        IntersectQuery floorQuery;
        IntersectInfo floorInfo;
        floorQuery.m_From = LTVector(vDesired.x, vPos.y + 80.0f, vDesired.z);
        floorQuery.m_To   = LTVector(vDesired.x, vPos.y - 530.0f, vDesired.z);
        floorQuery.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID | INTERSECT_HPOLY;

        if(g_pLTServer->IntersectSegment(&floorQuery, &floorInfo) &&
           floorInfo.m_hObject &&
           g_pLTSPhysics->IsWorldObject(floorInfo.m_hObject) == LT_YES)
        {
            vDesired.y = floorInfo.m_Point.y + 53.0f;
        }
        else
        {
            vDesired.y = vPos.y;
        }

        LTVector vZero(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vZero);
        g_pLTServer->MoveObject(m_hObject, &vDesired);
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
            }

            // NOLF2 CAIHuman's authored default half-dimensions are 24 x 53 x 24.
            // Do not inherit the HARMGuard animation's collision box on a CA map.
            LTVector vHumanDims(24.0f, 53.0f, 24.0f);
            g_pLTSPhysics->SetObjectDims(m_hObject, &vHumanDims, 0);

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