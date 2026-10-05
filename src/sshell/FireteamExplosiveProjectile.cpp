#include "FireteamExplosiveProjectile.h"

#include "serverinterfaces.h"
#include "serverutilities.h"
#include "msgids.h"

#include <iltcommon.h>
#include <iltphysics.h>
#include <ltobjectcreate.h>

BEGIN_CLASS(FireteamExplosiveProjectile)
END_CLASS_DEFAULT_FLAGS(
    FireteamExplosiveProjectile,
    BaseClass,
    LTNULL,
    LTNULL,
    CF_ALWAYSLOAD)

FireteamExplosiveProjectile::FireteamExplosiveProjectile() :
    m_hOwner(LTNULL),
    m_nDamage(0),
    m_fSplashRadius(0.0f),
    m_fSpeed(0.0f),
    m_fFuseSeconds(8.0f),
    m_fExplodeAt(0.0f),
    m_bRocket(false),
    m_bExploded(false),
    m_bHaveLastPos(false),
    m_vLastPos(0.0f, 0.0f, 0.0f)
{
}

void FireteamExplosiveProjectile::Configure(
    HOBJECT hOwner,
    uint8 nDamage,
    float fSplashRadius,
    float fSpeed,
    float fFuseSeconds,
    bool bRocket)
{
    m_hOwner = hOwner;
    m_nDamage = nDamage;
    m_fSplashRadius = fSplashRadius;
    m_fSpeed = fSpeed;
    m_fFuseSeconds = (fFuseSeconds > 0.01f) ? fFuseSeconds : 0.01f;
    m_fExplodeAt = g_pLTServer->GetTime() + m_fFuseSeconds;
    m_bRocket = bRocket;
    m_bExploded = false;

    uint32 nFlags = FLAG_VISIBLE |
                    FLAG_SOLID |
                    FLAG_TOUCH_NOTIFY |
                    FLAG_FORCECLIENTUPDATE;

    if(!m_bRocket)
    {
        nFlags |= FLAG_GRAVITY;
    }

    g_pLTSCommon->SetObjectFlags(
        m_hObject,
        OFT_Flags,
        nFlags,
        FLAG_VISIBLE |
        FLAG_SOLID |
        FLAG_TOUCH_NOTIFY |
        FLAG_FORCECLIENTUPDATE |
        FLAG_GRAVITY);

    LTRotation rRot;
    g_pLTServer->GetObjectRotation(m_hObject, &rRot);

    LTVector vVelocity = rRot.Forward() * m_fSpeed;
    if(!m_bRocket)
    {
        vVelocity.y += 180.0f;
    }

    g_pLTSPhysics->SetVelocity(m_hObject, &vVelocity);

    g_pLTServer->GetObjectPos(m_hObject, &m_vLastPos);
    m_bHaveLastPos = true;
    g_pLTServer->SetNextUpdate(m_hObject, 0.01f);

    g_pLTServer->CPrint(
        "Fireteam explosive: armed %s speed=%.1f fuse=%.2f radius=%.1f",
        m_bRocket ? "rocket" : "grenade",
        m_fSpeed,
        m_fFuseSeconds,
        m_fSplashRadius);
}

uint32 FireteamExplosiveProjectile::EngineMessageFn(
    uint32 messageID,
    void *pData,
    LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            BaseClass::EngineMessageFn(
                messageID,
                pData,
                fData);

            ObjectCreateStruct *pOCS =
                (ObjectCreateStruct*)pData;

            if(pOCS)
            {
                pOCS->m_ObjectType = OT_MODEL;
                pOCS->m_Flags |= FLAG_VISIBLE |
                                 FLAG_SOLID |
                                 FLAG_TOUCH_NOTIFY |
                                 FLAG_FORCECLIENTUPDATE |
                                 FLAG_REMOVEIFOUTSIDE;
            }

            return 1;
        }

        case MID_INITIALUPDATE:
        {
            LTVector vDims(4.0f, 4.0f, 4.0f);
            g_pLTSPhysics->SetObjectDims(
                m_hObject,
                &vDims,
                0);
            g_pLTSPhysics->SetForceIgnoreLimit(
                m_hObject,
                0.0f);
            g_pLTServer->SetNextUpdate(
                m_hObject,
                0.01f);
            return 1;
        }

        case MID_UPDATE:
            UpdateProjectile();
            if(!m_bExploded)
            {
                g_pLTServer->SetNextUpdate(
                    m_hObject,
                    0.01f);
            }
            return 1;

        case MID_TOUCHNOTIFY:
            HandleTouch((HOBJECT)pData);
            return 1;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(
        messageID,
        pData,
        fData);
}

void FireteamExplosiveProjectile::UpdateProjectile()
{
    if(m_bExploded)
    {
        return;
    }

    if(m_bRocket)
    {
        LTVector vCurrentPos;
        g_pLTServer->GetObjectPos(m_hObject, &vCurrentPos);

        if(m_bHaveLastPos)
        {
            LTVector vTravel = vCurrentPos - m_vLastPos;
            if(vTravel.MagSqr() > 0.01f)
            {
                IntersectQuery query;
                IntersectInfo info;
                query.m_From = m_vLastPos;
                query.m_To = vCurrentPos;
                query.m_Flags =
                    INTERSECT_OBJECTS |
                    IGNORE_NONSOLID |
                    INTERSECT_HPOLY;

                if(g_pLTServer->IntersectSegment(&query, &info))
                {
                    if(info.m_hObject != m_hOwner &&
                       info.m_hObject != m_hObject)
                    {
                        g_pLTServer->SetObjectPos(m_hObject, &info.m_Point);
                        Explode();
                        return;
                    }
                }
            }
        }

        m_vLastPos = vCurrentPos;
        m_bHaveLastPos = true;

        LTRotation rRot;
        g_pLTServer->GetObjectRotation(m_hObject, &rRot);
        LTVector vVelocity = rRot.Forward() * m_fSpeed;
        g_pLTSPhysics->SetVelocity(m_hObject, &vVelocity);
    }

    if(g_pLTServer->GetTime() >= m_fExplodeAt)
    {
        Explode();
    }
}

void FireteamExplosiveProjectile::HandleTouch(HOBJECT hObject)
{
    if(m_bExploded || !hObject || hObject == m_hOwner)
    {
        return;
    }

    if(m_bRocket)
    {
        Explode();
    }
}

void FireteamExplosiveProjectile::Explode()
{
    if(m_bExploded)
    {
        return;
    }

    m_bExploded = true;

    LTVector vExplosion;
    g_pLTServer->GetObjectPos(m_hObject, &vExplosion);

    HCLASS hZombieClass = g_pLTServer->GetClass("FireteamZombie");
    HCLASS hSealClass = g_pLTServer->GetClass("Seal");

    uint32 nEnemiesHit = 0;
    uint32 nTotalDamage = 0;

    HOBJECT hObject = g_pLTServer->GetNextObject(LTNULL);
    while(hObject)
    {
        // Damage can destroy an infected immediately. Capture the iterator
        // successor before sending damage so object removal cannot invalidate
        // the server object walk.
        HOBJECT hNextObject = g_pLTServer->GetNextObject(hObject);

        HCLASS hClass = g_pLTServer->GetObjectClass(hObject);
        if(!hClass)
        {
            hObject = hNextObject;
            continue;
        }

        const bool bEnemy =
            (hZombieClass && g_pLTServer->IsKindOf(hClass, hZombieClass)) ||
            (hSealClass && g_pLTServer->IsKindOf(hClass, hSealClass));

        if(!bEnemy)
        {
            hObject = hNextObject;
            continue;
        }

        LTVector vTarget;
        g_pLTServer->GetObjectPos(hObject, &vTarget);
        const float fDistance = (vTarget - vExplosion).Mag();

        if(fDistance > m_fSplashRadius)
        {
            hObject = hNextObject;
            continue;
        }

        float fScale = 1.0f;
        if(m_fSplashRadius > 1.0f)
        {
            fScale = 1.0f - (fDistance / m_fSplashRadius);
            if(fScale < 0.25f)
            {
                fScale = 0.25f;
            }
        }

        uint8 nAppliedDamage = (uint8)((float)m_nDamage * fScale);
        if(nAppliedDamage == 0)
        {
            nAppliedDamage = 1;
        }

        ++nEnemiesHit;
        nTotalDamage += nAppliedDamage;

        ILTMessage_Write *pDamage = LTNULL;
        if(g_pLTSCommon->CreateMessage(pDamage) == LT_OK && pDamage)
        {
            pDamage->IncRef();
            pDamage->Writeuint32(OBJ_MID_DAMAGE);
            pDamage->Writeuint8(nAppliedDamage);
            g_pLTServer->SendToObject(
                pDamage->Read(),
                m_hOwner,
                hObject,
                0);
            pDamage->DecRef();
        }

        hObject = hNextObject;
    }

    g_pLTServer->CPrint(
        "Fireteam explosive: %s exploded, enemies=%u totalDamage=%u",
        m_bRocket ? "rocket" : "grenade",
        nEnemiesHit,
        nTotalDamage);

    // Placeholder Jupiter effect until the matching Combat Arms explosion FX
    // is reproduced in Fireteam's compatible ClientFX database.
    PlayClientFX("CanImpact", m_hObject, LTNULL, LTNULL, 0);
    g_pLTServer->RemoveObject(m_hObject);
}
