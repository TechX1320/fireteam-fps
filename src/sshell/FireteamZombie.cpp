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
#include <stdio.h>

BEGIN_CLASS(FireteamZombie)
END_CLASS_DEFAULT_FLAGS(FireteamZombie, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

static uint32 s_nZombieSerial = 0;

FireteamZombie::FireteamZombie() :
    m_nHealth(0),
    m_fAttackCooldown(0.0f),
    m_fRepathCooldown(0.0f),
    m_fStuckTime(0.0f),
    m_nPathLane((s_nZombieSerial++) % 3),
    m_nWaypoint(0),
    m_hFace(LTNULL),
    m_hFaceAttachment(LTNULL),
    m_bDefLoaded(false)
{
    m_vLastPos.Init(0.0f, 0.0f, 0.0f);

    m_bDefLoaded =
        FT_LoadDefaultInfectedDef(
            "config/infected.cfg",
            m_Def);

    if(m_bDefLoaded)
    {
        m_nHealth = m_Def.nHealth;
    }
}

FireteamZombie::~FireteamZombie()
{
    if(m_hFace)
    {
        g_pLTServer->RemoveObject(m_hFace);
        m_hFace = LTNULL;
        m_hFaceAttachment = LTNULL;
    }
}

static bool FT_ZombieAssetExists(const char *pPath)
{
    FILE *pFile = fopen(pPath, "rb");
    if(!pFile)
    {
        return false;
    }

    fclose(pFile);
    return true;
}

void FireteamZombie::CreateInfectedFace()
{
    if(m_hFace ||
       !m_bDefLoaded ||
       !m_Def.sFaceModel[0] ||
       !m_Def.sFaceSocket[0])
    {
        return;
    }

    char sLocalFacePath[256];
    sprintf(
        sLocalFacePath,
        "rez/%s",
        m_Def.sFaceModel);

    if(!FT_ZombieAssetExists(sLocalFacePath))
    {
        g_pLTServer->CPrint(
            "Fireteam infected: face asset is not staged: %s",
            m_Def.sFaceModel);
        return;
    }

    HCLASS hBaseClass = g_pLTServer->GetClass("BaseClass");
    if(!hBaseClass)
    {
        return;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Flags = FLAG_VISIBLE |
                  FLAG_FORCECLIENTUPDATE |
                  FLAG_SHADOW;

    FT_CopyInfectedString(
        ocs.m_Filename,
        MAX_CS_FILENAME_LEN,
        m_Def.sFaceModel);
    FT_CopyInfectedString(
        ocs.m_SkinName,
        MAX_CS_FILENAME_LEN,
        m_Def.sFaceTexture);

    BaseClass *pFace =
        (BaseClass*)g_pLTServer->CreateObject(
            hBaseClass,
            &ocs);

    if(!pFace)
    {
        g_pLTServer->CPrint(
            "Fireteam infected: failed to create face %s.",
            m_Def.sFaceModel);
        return;
    }

    m_hFace = pFace->m_hObject;

    LTVector vOffset(
        m_Def.fFacePosX,
        m_Def.fFacePosY,
        m_Def.fFacePosZ);

    LTRotation rOffset;
    rOffset.Init();
    rOffset.Rotate(
        rOffset.Right(),
        MATH_DEGREES_TO_RADIANS(m_Def.fFaceRotX));
    rOffset.Rotate(
        rOffset.Up(),
        MATH_DEGREES_TO_RADIANS(m_Def.fFaceRotY));
    rOffset.Rotate(
        rOffset.Forward(),
        MATH_DEGREES_TO_RADIANS(m_Def.fFaceRotZ));

    LTRESULT result =
        g_pLTServer->CreateAttachment(
            m_hObject,
            m_hFace,
            m_Def.sFaceSocket,
            &vOffset,
            &rOffset,
            &m_hFaceAttachment);

    if(result != LT_OK)
    {
        g_pLTServer->CPrint(
            "Fireteam infected: face attachment failed on socket %s.",
            m_Def.sFaceSocket);
        g_pLTServer->RemoveObject(m_hFace);
        m_hFace = LTNULL;
        m_hFaceAttachment = LTNULL;
        return;
    }

    g_pLTServer->CPrint(
        "Fireteam infected: attached %s on %s rot=%.1f %.1f %.1f.",
        m_Def.sFaceModel,
        m_Def.sFaceSocket,
        m_Def.fFaceRotX,
        m_Def.fFaceRotY,
        m_Def.fFaceRotZ);
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
    const float kUpdate =
        (m_Def.fUpdateSeconds > 0.0f)
        ? m_Def.fUpdateSeconds
        : 0.10f;
    const float kMoveSpeed = m_Def.fRunSpeed;
    const float kAttackRange = m_Def.fAttackRange;

    // Navigation mechanics, not character content.
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
                pPlayer->ApplyDamage(m_Def.nAttackDamage);
            }
            m_fAttackCooldown = m_Def.fAttackCooldown;
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

                if(!m_bDefLoaded)
                {
                    g_pLTServer->CPrint(
                        "Fireteam infected: could not load config/infected.cfg.");
                }
                else
                {
                    FT_CopyInfectedString(
                        pOCS->m_Filenames[0],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyModel);

                    if(m_Def.sAnimationModel[0])
                    {
                        FT_CopyInfectedString(
                            pOCS->m_Filenames[1],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sAnimationModel);
                    }

                    FT_CopyInfectedString(
                        pOCS->m_SkinNames[0],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyTexture0);
                    FT_CopyInfectedString(
                        pOCS->m_SkinNames[1],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyTexture1);
                }
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            g_pLTServer->SetObjectColor(
                m_hObject,
                1.0f,
                1.0f,
                1.0f,
                1.0f);

            if(m_bDefLoaded)
            {
                g_pLTServer->CPrint(
                    "Fireteam infected: content %s body=%s health=%u speed=%.1f",
                    m_Def.sId,
                    m_Def.sBodyModel,
                    (uint32)m_Def.nHealth,
                    m_Def.fRunSpeed);

                if(m_Def.sIdleAnim[0])
                {
                    HMODELANIM hAnim =
                        g_pLTServer->GetAnimIndex(
                            m_hObject,
                            m_Def.sIdleAnim);

                    if(hAnim != INVALID_MODEL_ANIM)
                    {
                        g_pLTSModel->SetCurAnim(
                            m_hObject,
                            MAIN_TRACKER,
                            hAnim);
                        g_pLTSModel->SetLooping(
                            m_hObject,
                            MAIN_TRACKER,
                            LTTRUE);
                    }
                    else
                    {
                        g_pLTServer->CPrint(
                            "Fireteam infected: animation missing: %s",
                            m_Def.sIdleAnim);
                    }
                }

                CreateInfectedFace();

                LTVector vHumanDims(
                    m_Def.fCollisionX,
                    m_Def.fCollisionY,
                    m_Def.fCollisionZ);
                g_pLTSPhysics->SetObjectDims(
                    m_hObject,
                    &vHumanDims,
                    0);
            }

            g_pLTServer->GetObjectPos(m_hObject, &m_vLastPos);
            g_pLTServer->SetNextUpdate(
                m_hObject,
                m_Def.fUpdateSeconds > 0.0f
                    ? m_Def.fUpdateSeconds
                    : 0.10f);
        }
        break;

        case MID_UPDATE:
            UpdateZombie();
            g_pLTServer->SetNextUpdate(
                m_hObject,
                m_Def.fUpdateSeconds > 0.0f
                    ? m_Def.fUpdateSeconds
                    : 0.10f);
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
            g_pLTServer->CPrint(
                "Fireteam: infected %s killed.",
                m_bDefLoaded ? m_Def.sId : "unknown");
            g_pLTServer->RemoveObject(m_hObject);
            return 1;
        }
    }

    return BaseClass::ObjectMessageFn(hSender, pMsg);
}