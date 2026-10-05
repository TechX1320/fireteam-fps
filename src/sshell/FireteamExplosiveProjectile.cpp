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
    m_bRocket(false),
    m_bExploded(false)
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
    m_fFuseSeconds = fFuseSeconds;
    m_bRocket = bRocket;

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
                0.05f);
            return 1;
        }

        case MID_UPDATE:
            UpdateProjectile();
            if(!m_bExploded)
            {
                g_pLTServer->SetNextUpdate(
                    m_hObject,
                    0.05f);
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

    m_fFuseSeconds -= 0.05f;

    if(m_bRocket)
    {
        LTRotation rRot;
        g_pLTServer->GetObjectRotation(m_hObject, &rRot);
        LTVector vVelocity = rRot.Forward() * m_fSpeed;
        g_pLTSPhysics->SetVelocity(m_hObject, &vVelocity);
    }

    if(m_fFuseSeconds <= 0.0f)
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

    for(HOBJECT hObject = g_pLTServer->GetNextObject(LTNULL);
        hObject;
        hObject = g_pLTServer->GetNextObject(hObject))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObject);
        if(!hClass)
        {
            continue;
        }

        const bool bEnemy =
            (hZombieClass && g_pLTServer->IsKindOf(hClass, hZombieClass)) ||
            (hSealClass && g_pLTServer->IsKindOf(hClass, hSealClass));

        if(!bEnemy)
        {
            continue;
        }

        LTVector vTarget;
        g_pLTServer->GetObjectPos(hObject, &vTarget);
        const float fDistance = (vTarget - vExplosion).Mag();

        if(fDistance > m_fSplashRadius)
        {
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
    }

    PlayClientFX("CanImpact", m_hObject, LTNULL, LTNULL, 0);
    g_pLTServer->RemoveObject(m_hObject);
}
