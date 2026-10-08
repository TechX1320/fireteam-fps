#include "FireteamSpawner.h"
#include "serverinterfaces.h"
#include "msgids.h"
#include "FireteamDifficultyDefs.h"
#include "FireteamMutationBox.h"

#include <iltcommon.h>
#include <iltmessage.h>
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

class ObjectSpawnPoint : public BaseClass
{
public:
    ObjectSpawnPoint()
    {
    }
};

BEGIN_CLASS(ObjectSpawnPoint)
END_CLASS_DEFAULT_FLAGS(ObjectSpawnPoint, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

static const uint32 kMaxPerimeterSpawners = 64;
static const uint32 kSealCrawlerChancePercent = 2;

static HOBJECT s_hController = LTNULL;
static HOBJECT s_hPerimeterSpawners[kMaxPerimeterSpawners];
static uint32 s_nPerimeterSpawnerCount = 0;
static int s_nLastSpawner = -1;

static bool s_bRoundActive = false;
static bool s_bRoundIntermission = false;
static bool s_bGameOver = false;
static bool s_bQaZombiesEnabled = true;
static uint32 s_nRound = 0;
static uint32 s_nRoundTarget = 0;
static uint32 s_nRoundSpawned = 0;
static uint32 s_nRoundAlive = 0;
static uint32 s_nRoundKilled = 0;
static uint32 s_nMaxAlive = 0;
static float s_fNextSpawnTime = 0.0f;
static float s_fNextRoundTime = 0.0f;
static float s_fSpawnInterval = 1.25f;
static FTDifficultyDef s_Difficulty;

static void FT_BroadcastRoundState(uint8 nState)
{
    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) != LT_OK ||
       !pMsg)
    {
        return;
    }

    pMsg->IncRef();
    pMsg->Writeuint8(MSG_SC_ROUND);
    pMsg->Writeuint8(nState);
    pMsg->Writeuint16((uint16)s_nRound);
    pMsg->Writeuint16((uint16)s_nRoundTarget);
    pMsg->Writeuint16((uint16)s_nRoundKilled);
    pMsg->Writeuint16((uint16)s_nRoundAlive);

    g_pLTServer->SendToClient(
        pMsg->Read(),
        LTNULL,
        MESSAGE_GUARANTEED);

    pMsg->DecRef();
}

bool FT_AreQaZombiesEnabled()
{
    return s_bQaZombiesEnabled;
}

void FT_SetQaZombiesEnabled(
    bool bEnabled)
{
    if(s_bQaZombiesEnabled ==
       bEnabled)
    {
        return;
    }

    s_bQaZombiesEnabled =
        bEnabled;

    s_bRoundActive = false;
    s_bRoundIntermission = false;
    s_bGameOver = false;
    s_nRound = 0;
    s_nRoundTarget = 0;
    s_nRoundSpawned = 0;
    s_nRoundAlive = 0;
    s_nRoundKilled = 0;
    s_nMaxAlive = 0;
    s_fNextSpawnTime = 0.0f;
    s_fNextRoundTime = 0.0f;

    if(!bEnabled)
    {
        const uint32 kMaxQaClear =
            512;
        HOBJECT aRemove[kMaxQaClear];
        uint32 nRemove = 0;

        HCLASS hZombieClass =
            g_pLTServer->GetClass(
                "FireteamZombie");

        if(hZombieClass)
        {
            for(HOBJECT hObj =
                    g_pLTServer->GetNextObject(
                        LTNULL);
                hObj &&
                nRemove < kMaxQaClear;
                hObj =
                    g_pLTServer->GetNextObject(
                        hObj))
            {
                HCLASS hClass =
                    g_pLTServer->GetObjectClass(
                        hObj);

                if(hClass &&
                   g_pLTServer->IsKindOf(
                       hClass,
                       hZombieClass))
                {
                    aRemove[nRemove++] =
                        hObj;
                }
            }
        }

        for(uint32 i = 0;
            i < nRemove;
            ++i)
        {
            g_pLTServer->RemoveObject(
                aRemove[i]);
        }

        g_pLTServer->CPrint(
            "Fireteam QA: zombies OFF; cleared %u infected.",
            nRemove);
    }
    else
    {
        g_pLTServer->CPrint(
            "Fireteam QA: zombies ON; round controller restarting.");
    }

    FT_BroadcastRoundState(
        0);
}

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
    return (_strnicmp(
        m_sName,
        "Spawner_",
        8) == 0);
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
    s_bGameOver = false;
    s_nRound = 0;
    s_nRoundTarget = 0;
    s_nRoundSpawned = 0;
    s_nRoundAlive = 0;
    s_nRoundKilled = 0;
    s_nMaxAlive = 0;
    s_fNextSpawnTime = 0.0f;
    s_fNextRoundTime = 0.0f;
    s_fSpawnInterval = 1.25f;

    if(!FT_LoadActiveDifficulty(
        "config/difficulties.cfg",
        "config/session.cfg",
        s_Difficulty))
    {
        FT_InitDifficultyDefaults(
            s_Difficulty);
    }

    g_pLTServer->CPrint(
        "Fireteam difficulty: %s hp=%.2fx speed=%.2fx damage=%.2fx",
        s_Difficulty.sId,
        s_Difficulty.fHealthMultiplier,
        s_Difficulty.fSpeedMultiplier,
        s_Difficulty.fDamageMultiplier);

    srand((unsigned int)time(LTNULL));
}

void Spawner::CollectPerimeterSpawners()
{
    s_nPerimeterSpawnerCount = 0;

    const char *pSource =
        "Spawner";

    HCLASS hSpawnerClass =
        g_pLTServer->GetClass(
            "Spawner");

    if(hSpawnerClass)
    {
        for(HOBJECT hObj =
                g_pLTServer->GetNextObject(
                    LTNULL);
            hObj &&
            s_nPerimeterSpawnerCount <
                kMaxPerimeterSpawners;
            hObj =
                g_pLTServer->GetNextObject(
                    hObj))
        {
            HCLASS hClass =
                g_pLTServer->GetObjectClass(
                    hObj);

            if(!hClass ||
               !g_pLTServer->IsKindOf(
                   hClass,
                   hSpawnerClass))
            {
                continue;
            }

            Spawner *pSpawner =
                (Spawner*)
                g_pLTServer->HandleToObject(
                    hObj);

            if(pSpawner &&
               pSpawner->IsPerimeterSpawner())
            {
                s_hPerimeterSpawners[
                    s_nPerimeterSpawnerCount++] =
                    hObj;
            }
        }
    }

    // Non-FireTeam CA maps frequently use ObjectSpawnPoint instead of the
    // FireTeam Spawner objects. Keep those objects loadable and use them as a
    // generic zombie-map fallback so maps like Junk Flea can run rounds.
    if(s_nPerimeterSpawnerCount == 0)
    {
        HCLASS hObjectSpawnClass =
            g_pLTServer->GetClass(
                "ObjectSpawnPoint");

        if(hObjectSpawnClass)
        {
            pSource =
                "ObjectSpawnPoint";

            for(HOBJECT hObj =
                    g_pLTServer->GetNextObject(
                        LTNULL);
                hObj &&
                s_nPerimeterSpawnerCount <
                    kMaxPerimeterSpawners;
                hObj =
                    g_pLTServer->GetNextObject(
                        hObj))
            {
                HCLASS hClass =
                    g_pLTServer->GetObjectClass(
                        hObj);

                if(hClass &&
                   g_pLTServer->IsKindOf(
                       hClass,
                       hObjectSpawnClass))
                {
                    s_hPerimeterSpawners[
                        s_nPerimeterSpawnerCount++] =
                        hObj;
                }
            }
        }
    }

    // A few maps have AI patrol nodes but no explicit object spawns.
    if(s_nPerimeterSpawnerCount == 0)
    {
        HCLASS hPatrolClass =
            g_pLTServer->GetClass(
                "AINodePatrol");

        if(hPatrolClass)
        {
            pSource =
                "AINodePatrol";

            for(HOBJECT hObj =
                    g_pLTServer->GetNextObject(
                        LTNULL);
                hObj &&
                s_nPerimeterSpawnerCount <
                    kMaxPerimeterSpawners;
                hObj =
                    g_pLTServer->GetNextObject(
                        hObj))
            {
                HCLASS hClass =
                    g_pLTServer->GetObjectClass(
                        hObj);

                if(hClass &&
                   g_pLTServer->IsKindOf(
                       hClass,
                       hPatrolClass))
                {
                    s_hPerimeterSpawners[
                        s_nPerimeterSpawnerCount++] =
                        hObj;
                }
            }
        }
    }

    // Last-resort compatibility for simple custom maps.
    if(s_nPerimeterSpawnerCount == 0)
    {
        HCLASS hStartClass =
            g_pLTServer->GetClass(
                "GameStartPoint");

        if(hStartClass)
        {
            pSource =
                "GameStartPoint";

            for(HOBJECT hObj =
                    g_pLTServer->GetNextObject(
                        LTNULL);
                hObj &&
                s_nPerimeterSpawnerCount <
                    kMaxPerimeterSpawners;
                hObj =
                    g_pLTServer->GetNextObject(
                        hObj))
            {
                HCLASS hClass =
                    g_pLTServer->GetObjectClass(
                        hObj);

                if(hClass &&
                   g_pLTServer->IsKindOf(
                       hClass,
                       hStartClass))
                {
                    s_hPerimeterSpawners[
                        s_nPerimeterSpawnerCount++] =
                        hObj;
                }
            }
        }
    }

    g_pLTServer->CPrint(
        "Fireteam: registered %u round spawn anchors from %s.",
        s_nPerimeterSpawnerCount,
        pSource);
}

static bool FT_SpawnZombieAt(
    HOBJECT hSpawnObject)
{
    if(!hSpawnObject)
    {
        return false;
    }

    HCLASS hZombieClass =
        g_pLTServer->GetClass(
            "FireteamZombie");

    if(!hZombieClass)
    {
        return false;
    }

    LTVector vBasePos;
    LTRotation rBaseRot;

    g_pLTServer->GetObjectPos(
        hSpawnObject,
        &vBasePos);
    g_pLTServer->GetObjectRotation(
        hSpawnObject,
        &rBaseRot);

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Pos = vBasePos;
    ocs.m_Pos.x +=
        (float)((rand() % 29) - 14);
    ocs.m_Pos.z +=
        (float)((rand() % 29) - 14);
    ocs.m_Pos.y +=
        100.0f;
    ocs.m_Rotation =
        rBaseRot;

    return g_pLTServer->CreateObject(
        hZombieClass,
        &ocs) != LTNULL;
}

bool Spawner::SpawnZombie()
{
    return FT_SpawnZombieAt(
        m_hObject);
}

bool Spawner::SpawnCrawlerSeal()
{
    HCLASS hSealClass =
        g_pLTServer->GetClass("Seal");

    if(!hSealClass)
    {
        return false;
    }

    // The easter egg belongs inside the cabin rather than entering through a
    // perimeter infected spawner. Reuse the map's authored player starts so
    // this remains correct if the supported map has multiple spawn points.
    static const uint32 kMaxPlayerStarts = 32;
    HOBJECT aPlayerStarts[kMaxPlayerStarts];
    uint32 nPlayerStartCount = 0;

    HCLASS hStartClass =
        g_pLTServer->GetClass(
            "GameStartPoint");

    if(hStartClass)
    {
        for(HOBJECT hObj =
                g_pLTServer->GetNextObject(
                    LTNULL);
            hObj &&
            nPlayerStartCount <
                kMaxPlayerStarts;
            hObj =
                g_pLTServer->GetNextObject(
                    hObj))
        {
            HCLASS hClass =
                g_pLTServer->GetObjectClass(
                    hObj);

            if(hClass &&
               g_pLTServer->IsKindOf(
                   hClass,
                   hStartClass))
            {
                aPlayerStarts[
                    nPlayerStartCount++] =
                    hObj;
            }
        }
    }

    HOBJECT hSpawnPoint =
        nPlayerStartCount > 0
        ? aPlayerStarts[
            rand() % nPlayerStartCount]
        : m_hObject;

    LTVector vBasePos;
    LTRotation rBaseRot;

    g_pLTServer->GetObjectPos(
        hSpawnPoint,
        &vBasePos);
    g_pLTServer->GetObjectRotation(
        hSpawnPoint,
        &rBaseRot);

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    strcpy(
        ocs.m_Filename,
        "Models/seal.ltb");
    strcpy(
        ocs.m_SkinName,
        "ModelTextures/seal.dtx");

    ocs.m_Pos = vBasePos;
    ocs.m_Pos.x +=
        (float)((rand() % 31) - 15);
    ocs.m_Pos.z +=
        (float)((rand() % 31) - 15);

    // Give the legacy Seal::Spawn ground ray enough room to settle the model
    // without dropping it through the cabin floor.
    ocs.m_Pos.y += 48.0f;
    ocs.m_Rotation = rBaseRot;

    if(g_pLTServer->CreateObject(
        hSealClass,
        &ocs))
    {
        if(nPlayerStartCount > 0)
        {
            g_pLTServer->CPrint(
                "Fireteam: crawler seal easter egg spawned at cabin player start (%u available).",
                nPlayerStartCount);
        }
        else
        {
            g_pLTServer->CPrint(
                "Fireteam: crawler seal easter egg fallback spawn from %s; no GameStartPoint found.",
                m_sName);
        }

        return true;
    }

    return false;
}

void Spawner::StartNextRound()
{
    if(s_bGameOver)
    {
        return;
    }

    ++s_nRound;

    s_nRoundTarget =
        s_Difficulty.nRoundBase +
        ((s_nRound - 1) * s_Difficulty.nRoundGrowth);

    if(s_Difficulty.nRoundCap > 0 &&
       s_nRoundTarget > s_Difficulty.nRoundCap)
    {
        s_nRoundTarget =
            s_Difficulty.nRoundCap;
    }

    const uint32 nAliveSteps =
        (s_nRound - 1) /
        (s_Difficulty.nMaxAliveEvery > 0
            ? s_Difficulty.nMaxAliveEvery
            : 1);

    s_nMaxAlive =
        s_Difficulty.nMaxAliveBase +
        (nAliveSteps *
         s_Difficulty.nMaxAliveGrowth);

    if(s_Difficulty.nMaxAliveCap > 0 &&
       s_nMaxAlive > s_Difficulty.nMaxAliveCap)
    {
        s_nMaxAlive =
            s_Difficulty.nMaxAliveCap;
    }

    s_fSpawnInterval =
        s_Difficulty.fSpawnIntervalBase -
        ((float)(s_nRound - 1) *
         s_Difficulty.fSpawnIntervalDecay);

    if(s_fSpawnInterval <
       s_Difficulty.fSpawnIntervalMin)
    {
        s_fSpawnInterval =
            s_Difficulty.fSpawnIntervalMin;
    }

    s_nRoundSpawned = 0;
    s_nRoundAlive = 0;
    s_nRoundKilled = 0;
    s_bRoundActive = true;
    s_bRoundIntermission = false;
    s_fNextSpawnTime = g_pLTServer->GetTime() + 0.35f;

    g_pLTServer->CPrint(
        "Fireteam: %s ROUND %u START - %u infected, max %u alive, spawn %.2fs.",
        s_Difficulty.sId,
        s_nRound,
        s_nRoundTarget,
        s_nMaxAlive,
        s_fSpawnInterval);

    FT_BroadcastRoundState(1);
}

void Spawner::UpdateRoundController()
{
    if(s_hController != m_hObject)
    {
        return;
    }

    if(!s_bQaZombiesEnabled ||
       s_bGameOver)
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

    HOBJECT hSpawnObject =
        s_hPerimeterSpawners[
            nChoice];

    if(FT_SpawnZombieAt(
           hSpawnObject))
    {
        ++s_nRoundSpawned;
        ++s_nRoundAlive;
        s_nLastSpawner = nChoice;

        g_pLTServer->CPrint(
            "Fireteam: round %u spawn %u/%u from anchor %d/%u (%u alive).",
            s_nRound,
            s_nRoundSpawned,
            s_nRoundTarget,
            nChoice + 1,
            s_nPerimeterSpawnerCount,
            s_nRoundAlive);

        HCLASS hSpawnerClass =
            g_pLTServer->GetClass(
                "Spawner");

        HCLASS hSpawnClass =
            hSpawnObject
            ? g_pLTServer->GetObjectClass(
                hSpawnObject)
            : LTNULL;

        if(hSpawnerClass &&
           hSpawnClass &&
           g_pLTServer->IsKindOf(
               hSpawnClass,
               hSpawnerClass) &&
           (rand() % 100) <
               (int)kSealCrawlerChancePercent)
        {
            Spawner *pSpawn =
                (Spawner*)
                g_pLTServer->HandleToObject(
                    hSpawnObject);

            if(pSpawn)
            {
                pSpawn->SpawnCrawlerSeal();
            }
        }

        FT_BroadcastRoundState(0);
    }

    float fJitter = ((float)(rand() % 51) / 100.0f);
    s_fNextSpawnTime = fNow + s_fSpawnInterval + fJitter;
}

void FT_EnsureFireteamRoundController()
{
    // Use one tiny runtime Spawner as the map-independent round clock.
    // Authored Spawner/ObjectSpawnPoint objects remain spawn anchors only.
    s_hController =
        LTNULL;
    s_nPerimeterSpawnerCount =
        0;
    s_nLastSpawner =
        -1;

    HCLASS hSpawnerClass =
        g_pLTServer->GetClass(
            "Spawner");

    if(!hSpawnerClass)
    {
        g_pLTServer->CPrint(
            "Fireteam: round controller class unavailable.");
        return;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType =
        OT_NORMAL;

    HOBJECT hController =
        g_pLTServer->CreateObject(
            hSpawnerClass,
            &ocs);

    if(!hController)
    {
        g_pLTServer->CPrint(
            "Fireteam: failed to create generic round controller.");
        return;
    }

    g_pLTServer->CPrint(
        "Fireteam: generic round controller created.");
}


void FT_OnFireteamSquadGameOver()
{
    if(s_bGameOver)
    {
        return;
    }

    s_bGameOver = true;
    s_bRoundActive = false;
    s_bRoundIntermission = false;
    s_fNextSpawnTime = 0.0f;
    s_fNextRoundTime = 0.0f;

    g_pLTServer->CPrint(
        "Fireteam: GAME OVER on round %u - squad is out of lives.",
        s_nRound);

    FT_BroadcastRoundState(
        3);
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
            s_fNextRoundTime =
                g_pLTServer->GetTime() +
                s_Difficulty.fIntermissionSeconds;

            g_pLTServer->CPrint(
                "Fireteam: %s ROUND %u CLEAR - next round in %.1f seconds.",
                s_Difficulty.sId,
                s_nRound,
                s_Difficulty.fIntermissionSeconds);

            FT_BroadcastRoundState(2);

            FT_SpawnRoundClearMutationBoxes();
        }
        else
        {
            FT_BroadcastRoundState(0);
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
            if(!s_hController)
            {
                ResetRoundController();
            }

            g_pLTServer->SetNextUpdate(
                m_hObject,
                s_hController == m_hObject
                    ? 0.20f
                    : 0.0f);
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