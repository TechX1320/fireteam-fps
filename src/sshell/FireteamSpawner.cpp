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