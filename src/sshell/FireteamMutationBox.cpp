#include "FireteamMutationBox.h"
#include "FireteamPowerupDefs.h"
#include "playersrvr.h"
#include "FireteamZombie.h"
#include "serverinterfaces.h"

#include <iltcommon.h>
#include <iltphysics.h>
#include <iltsoundmgr.h>
#include <ltobjectcreate.h>
#include <stdlib.h>

BEGIN_CLASS(FireteamMutationBox)
END_CLASS_DEFAULT_FLAGS(
    FireteamMutationBox,
    BaseClass,
    LTNULL,
    LTNULL,
    CF_ALWAYSLOAD)

static FTPowerupDef s_MutationDef;
static bool s_bMutationDefLoaded = false;

static const FTPowerupDef&
FT_GetMutationDef()
{
    if(!s_bMutationDefLoaded)
    {
        FT_LoadPowerupDef(
            "config/powerups.cfg",
            s_MutationDef);

        s_bMutationDefLoaded = true;
    }

    return s_MutationDef;
}

static bool FT_MutationAssetExists(
    const char *pRelative)
{
    if(!pRelative ||
       !pRelative[0])
    {
        return false;
    }

    char sPath[256];

    sprintf(
        sPath,
        "rez/%s",
        pRelative);

    FILE *pFile =
        fopen(sPath, "rb");

    if(!pFile)
    {
        return false;
    }

    fclose(pFile);
    return true;
}

static HOBJECT FT_SpawnMutationBox(
    const LTVector &vPosition)
{
    const FTPowerupDef &def =
        FT_GetMutationDef();

    if(!def.bEnabled ||
       !FT_MutationAssetExists(
           def.sModel) ||
       !FT_MutationAssetExists(
           def.sTexture))
    {
        return LTNULL;
    }

    HCLASS hClass =
        g_pLTServer->GetClass(
            "FireteamMutationBox");

    if(!hClass)
    {
        return LTNULL;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();

    ocs.m_ObjectType =
        OT_MODEL;

    ocs.m_Pos =
        vPosition;

    // Let gravity settle the pickup onto the nearby floor.
    ocs.m_Pos.y += 28.0f;

    BaseClass *pObject =
        (BaseClass*)g_pLTServer->CreateObject(
            hClass,
            &ocs);

    HOBJECT hObject =
        pObject
        ? pObject->m_hObject
        : LTNULL;

    if(hObject)
    {
        g_pLTServer->CPrint(
            "Fireteam powerup: Mutation Box spawned %.1f %.1f %.1f.",
            ocs.m_Pos.x,
            ocs.m_Pos.y,
            ocs.m_Pos.z);
    }

    return hObject;
}

FireteamMutationBox::FireteamMutationBox()
    : m_fLifeRemaining(25.0f),
      m_bConsumed(false)
{
}

uint32 FireteamMutationBox::PreCreate(
    void *pData,
    float fData)
{
    BaseClass::EngineMessageFn(
        MID_PRECREATE,
        pData,
        fData);

    ObjectCreateStruct *pOCS =
        (ObjectCreateStruct*)pData;

    if(!pOCS)
    {
        return 1;
    }

    const FTPowerupDef &def =
        FT_GetMutationDef();

    pOCS->m_ObjectType =
        OT_MODEL;

    pOCS->m_Flags =
        FLAG_VISIBLE |
        FLAG_TOUCH_NOTIFY |
        FLAG_GRAVITY |
        FLAG_SHADOW;

    FT_CopyPowerupString(
        pOCS->m_Filename,
        MAX_CS_FILENAME_LEN,
        def.sModel);

    FT_CopyPowerupString(
        pOCS->m_SkinNames[0],
        MAX_CS_FILENAME_LEN,
        def.sTexture);

    if(def.sRenderStyle[0])
    {
        FT_CopyPowerupString(
            pOCS->m_RenderStyleNames[0],
            MAX_CS_FILENAME_LEN,
            def.sRenderStyle);
    }

    m_fLifeRemaining =
        def.fLifetimeSeconds > 0.0f
        ? def.fLifetimeSeconds
        : 25.0f;

    return 1;
}

uint32 FireteamMutationBox::InitialUpdate()
{
    LTVector vDims(
        18.0f,
        18.0f,
        18.0f);

    g_pLTSPhysics->SetObjectDims(
        m_hObject,
        &vDims,
        0);

    g_pLTServer->SetNextUpdate(
        m_hObject,
        0.25f);

    return 1;
}

uint32 FireteamMutationBox::Update()
{
    if(m_bConsumed)
    {
        g_pLTServer->RemoveObject(
            m_hObject);

        return 1;
    }

    m_fLifeRemaining -=
        0.25f;

    if(m_fLifeRemaining <= 0.0f)
    {
        g_pLTServer->RemoveObject(
            m_hObject);

        return 1;
    }

    g_pLTServer->SetNextUpdate(
        m_hObject,
        0.25f);

    return 1;
}

void FireteamMutationBox::PlayPickupSound()
{
    const FTPowerupDef &def =
        FT_GetMutationDef();

    if(!def.sPickupSound[0])
    {
        return;
    }

    PlaySoundInfo info;
    PLAYSOUNDINFO_INIT(info);

    info.m_dwFlags =
        PLAYSOUND_ATTACHED |
        PLAYSOUND_3D;

    info.m_hObject =
        m_hObject;

    info.m_fInnerRadius =
        48.0f;

    info.m_fOuterRadius =
        650.0f;

    info.m_nVolume =
        100;

    FT_CopyPowerupString(
        info.m_szSoundName,
        sizeof(info.m_szSoundName),
        def.sPickupSound);

    HLTSOUND hSound = LTNULL;

    g_pLTServer->SoundMgr()->
        PlaySound(
            &info,
            hSound);
}

static void FT_GrantTeamWallhack(
    float fSeconds)
{
    const float fRemaining =
        FT_ExtendZombieWallhack(
            fSeconds);

    if(fRemaining <= 0.0f)
    {
        return;
    }

    HCLASS hPlayerClass =
        g_pLTServer->GetClass(
            "CPlayerSrvr");

    if(!hPlayerClass)
    {
        return;
    }

    for(HOBJECT hObject =
            g_pLTServer->GetNextObject(
                LTNULL);
        hObject;
        hObject =
            g_pLTServer->GetNextObject(
                hObject))
    {
        HCLASS hClass =
            g_pLTServer->GetObjectClass(
                hObject);

        if(!hClass ||
           !g_pLTServer->IsKindOf(
                hClass,
                hPlayerClass))
        {
            continue;
        }

        CPlayerSrvr *pPlayer =
            (CPlayerSrvr*)
            g_pLTServer->HandleToObject(
                hObject);

        if(!pPlayer)
        {
            continue;
        }

        pPlayer->SyncPowerupState();
        pPlayer->NotifyPowerup(
            "MUTATION BOX: ZOMBIE WALLHACK - TEAM",
            4.0f);
    }
}

uint32 FireteamMutationBox::TouchNotify(
    void *pData,
    float)
{
    if(m_bConsumed)
    {
        return 1;
    }

    HOBJECT hObject =
        (HOBJECT)pData;

    if(!hObject)
    {
        return 1;
    }

    HCLASS hPlayerClass =
        g_pLTServer->GetClass(
            "CPlayerSrvr");

    HCLASS hObjectClass =
        g_pLTServer->GetObjectClass(
            hObject);

    if(!hPlayerClass ||
       !hObjectClass ||
       !g_pLTServer->IsKindOf(
            hObjectClass,
            hPlayerClass))
    {
        return 1;
    }

    CPlayerSrvr *pPlayer =
        (CPlayerSrvr*)
        g_pLTServer->HandleToObject(
            hObject);

    if(!pPlayer ||
       !pPlayer->IsAlive())
    {
        return 1;
    }

    const FTPowerupDef &def =
        FT_GetMutationDef();

    const uint32 nTotalWeight =
        def.nAmmoWeight +
        def.nHealthWeight +
        def.nBottomlessWeight +
        def.nOneHitWeight +
        def.nGodWeight +
        def.nWallhackWeight;

    if(nTotalWeight == 0)
    {
        return 1;
    }

    const uint32 nRoll =
        (uint32)(
            rand() %
            nTotalWeight);

    uint32 nCursor =
        def.nAmmoWeight;

    if(nRoll < nCursor)
    {
        pPlayer->GrantAmmoMagazines(
            def.nAmmoMagazines);

        pPlayer->NotifyPowerup(
            "MUTATION BOX: AMMO RESUPPLY",
            3.5f);
    }
    else if(nRoll <
            (nCursor +=
                def.nHealthWeight))
    {
        pPlayer->GrantHealth(
            def.nHealthAmount);

        pPlayer->NotifyPowerup(
            "MUTATION BOX: HEALTH BOOST",
            3.5f);
    }
    else if(nRoll <
            (nCursor +=
                def.nBottomlessWeight))
    {
        pPlayer->GrantBottomless(
            def.fBottomlessSeconds);

        pPlayer->NotifyPowerup(
            "MUTATION BOX: BOTTOMLESS MAG",
            4.0f);
    }
    else if(nRoll <
            (nCursor +=
                def.nOneHitWeight))
    {
        pPlayer->GrantOneHit(
            def.fOneHitSeconds);

        pPlayer->NotifyPowerup(
            "MUTATION BOX: ONE HIT KILL",
            4.0f);
    }
    else if(nRoll <
            (nCursor +=
                def.nGodWeight))
    {
        pPlayer->GrantGodMode(
            def.fGodSeconds);

        pPlayer->NotifyPowerup(
            "MUTATION BOX: GOD MODE",
            4.0f);
    }
    else
    {
        // Wallhack is deliberately the first explicit team/global powerup.
        // RenderStyle is authoritative on shared infected, so every player
        // receives the effect and HUD timer together.
        FT_GrantTeamWallhack(
            def.fWallhackSeconds);
    }

    m_bConsumed =
        true;

    g_pLTSCommon->SetObjectFlags(
        m_hObject,
        OFT_Flags,
        0,
        FLAG_TOUCH_NOTIFY |
        FLAG_VISIBLE);

    PlayPickupSound();

    // Keep the hidden object alive briefly so its attached GET.WAV starts.
    g_pLTServer->SetNextUpdate(
        m_hObject,
        0.60f);

    return 1;
}

uint32 FireteamMutationBox::EngineMessageFn(
    uint32 messageID,
    void *pData,
    float fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
            return PreCreate(
                pData,
                fData);

        case MID_INITIALUPDATE:
            return InitialUpdate();

        case MID_UPDATE:
            return Update();

        case MID_TOUCHNOTIFY:
            return TouchNotify(
                pData,
                fData);

        default:
            break;
    }

    return BaseClass::EngineMessageFn(
        messageID,
        pData,
        fData);
}

bool FT_MaybeSpawnMutationBoxOnKill(
    const LTVector &vPosition)
{
    const FTPowerupDef &def =
        FT_GetMutationDef();

    if(!def.bEnabled ||
       def.nKillDropChancePercent == 0)
    {
        return false;
    }

    if((uint32)(rand() % 100) >=
       def.nKillDropChancePercent)
    {
        return false;
    }

    return FT_SpawnMutationBox(
        vPosition) != LTNULL;
}

uint32 FT_SpawnRoundClearMutationBoxes()
{
    const FTPowerupDef &def =
        FT_GetMutationDef();

    if(!def.bEnabled)
    {
        return 0;
    }

    uint32 nMin =
        def.nRoundClearMin;

    uint32 nMax =
        def.nRoundClearMax;

    if(nMax < nMin)
    {
        const uint32 nTemp =
            nMin;

        nMin = nMax;
        nMax = nTemp;
    }

    const uint32 nCount =
        nMin +
        (nMax > nMin
            ? (uint32)(
                rand() %
                (nMax - nMin + 1))
            : 0);

    if(nCount == 0)
    {
        return 0;
    }

    HCLASS hPlayerClass =
        g_pLTServer->GetClass(
            "CPlayerSrvr");

    if(!hPlayerClass)
    {
        return 0;
    }

    HOBJECT hAnchor = LTNULL;

    for(HOBJECT hObject =
            g_pLTServer->GetNextObject(
                LTNULL);
        hObject;
        hObject =
            g_pLTServer->GetNextObject(
                hObject))
    {
        HCLASS hClass =
            g_pLTServer->GetObjectClass(
                hObject);

        if(!hClass ||
           !g_pLTServer->IsKindOf(
                hClass,
                hPlayerClass))
        {
            continue;
        }

        CPlayerSrvr *pPlayer =
            (CPlayerSrvr*)
            g_pLTServer->HandleToObject(
                hObject);

        if(pPlayer &&
           pPlayer->IsAlive())
        {
            hAnchor = hObject;
            break;
        }
    }

    if(!hAnchor)
    {
        return 0;
    }

    LTVector vAnchor;

    g_pLTServer->GetObjectPos(
        hAnchor,
        &vAnchor);

    uint32 nSpawned = 0;

    for(uint32 i = 0;
        i < nCount;
        ++i)
    {
        LTVector vSpawn =
            vAnchor;

        vSpawn.x +=
            (float)(
                (rand() % 181) -
                90);

        vSpawn.z +=
            (float)(
                (rand() % 181) -
                90);

        if(FT_SpawnMutationBox(
            vSpawn))
        {
            ++nSpawned;
        }
    }

    g_pLTServer->CPrint(
        "Fireteam powerup: round clear rolled %u Mutation Box(es), spawned %u.",
        nCount,
        nSpawned);

    return nSpawned;
}
