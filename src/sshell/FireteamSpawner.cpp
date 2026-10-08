#include "FireteamSpawner.h"
#include "FireteamZombie.h"
#include "FireteamNavigation.h"
#include "serverinterfaces.h"
#include "msgids.h"
#include "FireteamDifficultyDefs.h"
#include "FireteamMutationBox.h"
#include "playersrvr.h"

#include <iltcommon.h>
#include <iltmessage.h>
#include <iltphysics.h>
#include <ltobjectcreate.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
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
static float s_fMissingInfectedSince = 0.0f;
static uint32 s_nMissingInfectedRestores = 0;
static FTDifficultyDef s_Difficulty;

// The first round must never begin during map load, before any player exists.
// Late joins can extend preparation, but never indefinitely postpone a match.
static const float kFirstRoundReadySeconds = 45.0f;
static const float kLateJoinGraceSeconds = 15.0f;
static const float kMaximumFirstRoundReadySeconds = 60.0f;
static bool s_bFirstRoundPreparing = false;
static float s_fFirstPlayerJoinedAt = 0.0f;
static float s_fFirstRoundReadyAt = 0.0f;
static uint32 s_nLastPreparationSecond = 0xFFFFFFFF;

static void FT_SendFirstRoundPreparation(HCLIENT hClient, float fRemaining)
{
    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) != LT_OK || !pMsg)
        return;

    pMsg->IncRef();
    pMsg->Writeuint8(MSG_SC_ROUND_PREP);
    pMsg->Writefloat(fRemaining > 0.0f ? fRemaining : 0.0f);
    g_pLTServer->SendToClient(pMsg->Read(), hClient, MESSAGE_GUARANTEED);
    pMsg->DecRef();
}

static void FT_SendRoundState(uint8 nState, HCLIENT hClient)
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
        hClient,
        MESSAGE_GUARANTEED);

    pMsg->DecRef();
}

static void FT_BroadcastRoundState(uint8 nState)
{
    FT_SendRoundState(nState, LTNULL);
}

void FT_OnFireteamPlayerJoined(HCLIENT hClient)
{
    const float fNow = g_pLTServer->GetTime();
    if(s_nRound == 0 && !s_bGameOver && s_bQaZombiesEnabled)
    {
        if(!s_bFirstRoundPreparing)
        {
            s_bFirstRoundPreparing = true;
            s_fFirstPlayerJoinedAt = fNow;
            s_fFirstRoundReadyAt = fNow + kFirstRoundReadySeconds;
            g_pLTServer->CPrint(
                "Fireteam: first player entered; Round 1 preparation %.0f seconds.",
                kFirstRoundReadySeconds);
        }
        else
        {
            // Allow a joining teammate to get ready, but cap total waiting
            // time to prevent join/disconnect spam from delaying rounds.
            float fExtendUntil = fNow + kLateJoinGraceSeconds;
            const float fHardCap =
                s_fFirstPlayerJoinedAt + kMaximumFirstRoundReadySeconds;
            if(fExtendUntil > fHardCap)
                fExtendUntil = fHardCap;
            if(fExtendUntil > s_fFirstRoundReadyAt)
                s_fFirstRoundReadyAt = fExtendUntil;
        }

        s_nLastPreparationSecond = 0xFFFFFFFF;
        FT_SendFirstRoundPreparation(
            LTNULL,
            s_fFirstRoundReadyAt - fNow);
        return;
    }

    // Reconcile rounds for clients joining an already-running server.
    FT_SendRoundState(s_bGameOver ? 3 : 0, hClient);
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
    s_fMissingInfectedSince = 0.0f;
    s_nMissingInfectedRestores = 0;
    s_bFirstRoundPreparing = bEnabled;
    s_fFirstPlayerJoinedAt = g_pLTServer->GetTime();
    s_fFirstRoundReadyAt =
        bEnabled ? s_fFirstPlayerJoinedAt + 5.0f : 0.0f;
    s_nLastPreparationSecond = 0xFFFFFFFF;
    FT_SendFirstRoundPreparation(
        LTNULL, bEnabled ? 5.0f : 0.0f);

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
    s_fMissingInfectedSince = 0.0f;
    s_nMissingInfectedRestores = 0;
    s_bFirstRoundPreparing = false;
    s_fFirstPlayerJoinedAt = 0.0f;
    s_fFirstRoundReadyAt = 0.0f;
    s_nLastPreparationSecond = 0xFFFFFFFF;

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

// Generic CA maps may have authored spawners on several disconnected floors.
// Prefer a nearby, same-level anchor so infected do not spawn one kilometer
// below the squad and endlessly report waypoint=0/0.
static bool FT_HasAnyPlayerInWorld()
{
    HCLASS hPlayerClass = g_pLTServer->GetClass("CPlayerSrvr");
    if(!hPlayerClass)
        return false;

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(hClass && g_pLTServer->IsKindOf(hClass, hPlayerClass))
            return true;
    }
    return false;
}

static bool FT_SpawnAnchorNearLivingPlayer(HOBJECT hAnchor)
{
    if(!hAnchor)
        return false;

    HCLASS hPlayerClass = g_pLTServer->GetClass("CPlayerSrvr");
    if(!hPlayerClass)
        return false;

    LTVector vAnchor;
    g_pLTServer->GetObjectPos(hAnchor, &vAnchor);

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(!hClass || !g_pLTServer->IsKindOf(hClass, hPlayerClass))
            continue;

        CPlayerSrvr *pPlayer = (CPlayerSrvr*)g_pLTServer->HandleToObject(hObj);
        if(!pPlayer || !pPlayer->IsTargetable())
            continue;

        LTVector vPlayer;
        g_pLTServer->GetObjectPos(hObj, &vPlayer);
        float fY = vAnchor.y - vPlayer.y;
        if(fY < 0.0f)
            fY = -fY;

        const float fX = vAnchor.x - vPlayer.x;
        const float fZ = vAnchor.z - vPlayer.z;
        if(fY <= 260.0f && (fX * fX + fZ * fZ) <= 2400.0f * 2400.0f)
            return true;
    }

    return false;
}


// Compare the authoritative living-object roster to the remaining count.
// Lost/removed objects must be replaced, never treated as free kills.
static uint32 FT_CountLivingInfectedObjects()
{
    HCLASS hClass = g_pLTServer->GetClass("FireteamZombie");
    if(!hClass) return 0;
    uint32 nLiving = 0;
    for(HOBJECT h = g_pLTServer->GetNextObject(LTNULL); h;
        h = g_pLTServer->GetNextObject(h))
    {
        HCLASS hType = g_pLTServer->GetObjectClass(h);
        if(!hType || !g_pLTServer->IsKindOf(hType, hClass))
            continue;
        FireteamZombie *pZombie =
            (FireteamZombie*)g_pLTServer->HandleToObject(h);
        if(pZombie && pZombie->IsAliveForRound())
            ++nLiving;
    }
    return nLiving;
}

bool FT_IsFinalLivingInfected(HOBJECT hZombie)
{
    if(!hZombie || !s_bRoundActive || s_nRoundAlive != 1 ||
       s_nRoundTarget == 0 || s_nRoundSpawned != s_nRoundTarget)
        return false;
    HCLASS hClass = g_pLTServer->GetClass("FireteamZombie");
    if(!hClass) return false;
    uint32 nAlive = 0;
    bool bFound = false;
    for(HOBJECT h = g_pLTServer->GetNextObject(LTNULL); h;
        h = g_pLTServer->GetNextObject(h))
    {
        HCLASS hType = g_pLTServer->GetObjectClass(h);
        if(!hType || !g_pLTServer->IsKindOf(hType, hClass)) continue;
        FireteamZombie *pZombie =
            (FireteamZombie*)g_pLTServer->HandleToObject(h);
        if(!pZombie || !pZombie->IsAliveForRound()) continue;
        ++nAlive;
        if(h == hZombie) bFound = true;
        if(nAlive > 1) return false;
    }
    return bFound && nAlive == 1;
}

bool FT_TryRecoverFinalInfected(HOBJECT hZombie, HOBJECT hTarget,
                               LTVector &vResult)
{
    if(!hTarget || !FT_IsFinalLivingInfected(hZombie) ||
       s_nPerimeterSpawnerCount == 0) return false;

    LTVector vPlayer, vOld;
    g_pLTServer->GetObjectPos(hTarget, &vPlayer);
    g_pLTServer->GetObjectPos(hZombie, &vOld);

    const bool bHasNav = FT_GetNavigationVolumeCount() != 0;
    float fBest = FLT_MAX;
    int nBest = -1;
    LTVector vBest;
    bool bVerified = false;
    for(uint32 i = 0; i < s_nPerimeterSpawnerCount; ++i)
    {
        if(!s_hPerimeterSpawners[i]) continue;
        LTVector vPos;
        g_pLTServer->GetObjectPos(s_hPerimeterSpawners[i], &vPos);
        vPos.y += 100.0f; // Same height used by ordinary zombie spawns.
        if(fabs(vPos.y - vPlayer.y) > 260.0f) continue;
        LTVector vDelta = vPos - vPlayer;
        vDelta.y = 0.0f;
        const float fDistSq = vDelta.MagSqr();
        if(fDistSq < 450.0f * 450.0f ||
           fDistSq > 1850.0f * 1850.0f) continue;
        vDelta = vPos - vOld;
        vDelta.y = 0.0f;
        if(vDelta.MagSqr() < 300.0f * 300.0f) continue;
        std::vector<LTVector> waypoints;
        const bool bRoute = !bHasNav ||
            FT_BuildNavigationPath(vPos, vPlayer, 15.0f, 0, waypoints);
        const float fScore =
            (float)fabs(sqrt(fDistSq) - 850.0f) +
            (bRoute ? 0.0f : 2000.0f) +
            (float)waypoints.size() * 15.0f;
        if(fScore < fBest)
        {
            fBest = fScore;
            nBest = (int)i;
            vBest = vPos;
            bVerified = bRoute;
        }
    }
    if(nBest < 0)
    {
        g_pLTServer->CPrint(
            "Fireteam straggler: no nearby same-floor map spawner; zombie still alive at %.1f %.1f %.1f.",
            vOld.x, vOld.y, vOld.z);
        return false;
    }

    // Only reposition the same living object. No kill, bonus or free round.
    LTVector vZero(0.0f, 0.0f, 0.0f);
    g_pLTSPhysics->SetVelocity(hZombie, &vZero);
    g_pLTServer->SetObjectPos(hZombie, &vBest);
    g_pLTServer->GetObjectPos(hZombie, &vResult);
    g_pLTServer->CPrint(
        "Fireteam straggler: recovered final infected from %.1f %.1f %.1f to %.1f %.1f %.1f via anchor %d/%u route=%s kills=%u/%u.",
        vOld.x, vOld.y, vOld.z, vResult.x, vResult.y, vResult.z,
        nBest + 1, s_nPerimeterSpawnerCount,
        bVerified ? "verified" : "fallback",
        s_nRoundKilled, s_nRoundTarget);
    return true;
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

    // A Seal is a BONUS easter egg, never one of the round's infected.
    // Spawning it at GameStartPoint made it appear inside players' safe
    // rooms on Cabin Fever and BLACKLUNG; use the authorized perimeter
    // Spawner that triggered the easter egg instead.
    LTVector vBasePos;
    LTRotation rBaseRot;
    g_pLTServer->GetObjectPos(m_hObject, &vBasePos);
    g_pLTServer->GetObjectRotation(m_hObject, &rBaseRot);

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
        g_pLTServer->CPrint(
            "Fireteam: BONUS crawler Seal spawned from perimeter anchor %s (not counted in round ALIVE/KILLS).",
            m_sName);
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
    s_fMissingInfectedSince = 0.0f;
    s_nMissingInfectedRestores = 0;
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

    // Dedicated servers can be idle for hours. Reset preparation if the
    // squad disconnects and never spawn zombies into an empty world.
    if(!FT_HasAnyPlayerInWorld())
    {
        if(s_nRound == 0)
        {
            s_bFirstRoundPreparing = false;
            s_fFirstRoundReadyAt = 0.0f;
            s_nLastPreparationSecond = 0xFFFFFFFF;
        }
        return;
    }

    if(s_nPerimeterSpawnerCount == 0)
    {
        CollectPerimeterSpawners();
        if(s_nPerimeterSpawnerCount == 0) return;
    }

    const float fNow = g_pLTServer->GetTime();

    if(s_bRoundActive && s_nRoundAlive > 0 &&
       s_nRoundSpawned > 0 &&
       s_nMissingInfectedRestores < s_nRoundTarget)
    {
        const uint32 nPhysical = FT_CountLivingInfectedObjects();
        if(nPhysical < s_nRoundAlive)
        {
            if(s_fMissingInfectedSince <= 0.0f)
                s_fMissingInfectedSince = fNow;
            if(fNow - s_fMissingInfectedSince >= 5.0f)
            {
                HOBJECT hAnchor = LTNULL;
                for(uint32 i = 0; i < s_nPerimeterSpawnerCount; ++i)
                {
                    if(FT_SpawnAnchorNearLivingPlayer(s_hPerimeterSpawners[i]))
                    {
                        hAnchor = s_hPerimeterSpawners[i];
                        break;
                    }
                }
                if(!hAnchor)
                    hAnchor = s_hPerimeterSpawners[
                        (uint32)(rand() % s_nPerimeterSpawnerCount)];
                if(FT_SpawnZombieAt(hAnchor))
                {
                    ++s_nMissingInfectedRestores;
                    g_pLTServer->CPrint(
                        "Fireteam: restored missing infected object (%u physical, %u expected, restores %u/%u). No kill credited.",
                        nPhysical, s_nRoundAlive,
                        s_nMissingInfectedRestores, s_nRoundTarget);
                }
                s_fMissingInfectedSince = fNow;
            }
        }
        else
        {
            s_fMissingInfectedSince = 0.0f;
        }
    }

    if(s_nRound == 0)
    {
        // Without a player, the world may have loaded but gameplay cannot
        // begin. In particular, do not send ROUND START before UI connection.
        if(!s_bFirstRoundPreparing)
            return;

        if(fNow < s_fFirstRoundReadyAt)
        {
            const uint32 nSeconds =
                (uint32)(s_fFirstRoundReadyAt - fNow + 0.999f);
            if(nSeconds != s_nLastPreparationSecond)
            {
                s_nLastPreparationSecond = nSeconds;
                FT_SendFirstRoundPreparation(
                    LTNULL, s_fFirstRoundReadyAt - fNow);
            }
            return;
        }

        s_bFirstRoundPreparing = false;
        FT_SendFirstRoundPreparation(LTNULL, 0.0f);
        StartNextRound();
    }
    else if(!s_bRoundActive && !s_bRoundIntermission)
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

    // Select from reachable-looking local floor anchors first. A zero-match
    // fallback retains compatibility with large authored FireTeam maps.
    uint32 aEligible[kMaxPerimeterSpawners];
    uint32 nEligible = 0;
    for(uint32 i = 0; i < s_nPerimeterSpawnerCount; ++i)
    {
        if(FT_SpawnAnchorNearLivingPlayer(s_hPerimeterSpawners[i]))
            aEligible[nEligible++] = i;
    }

    int nChoice = 0;
    const uint32 nPool = nEligible > 0
        ? nEligible : s_nPerimeterSpawnerCount;

    if(nPool > 0)
    {
        for(uint32 nTry = 0; nTry < 8; ++nTry)
        {
            const uint32 nCandidate = (uint32)(rand() % nPool);
            nChoice = (int)(nEligible > 0
                ? aEligible[nCandidate] : nCandidate);
            if(nChoice != s_nLastSpawner || nPool == 1)
                break;
        }
    }

    if(nEligible == 0 && s_nRoundSpawned == 0)
    {
        g_pLTServer->CPrint(
            "Fireteam: no spawn anchor within 260Y/2400XZ of a living player; using map fallback.");
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
    s_bFirstRoundPreparing = false;
    s_fFirstRoundReadyAt = 0.0f;
    s_fFirstPlayerJoinedAt = 0.0f;
    s_nLastPreparationSecond = 0xFFFFFFFF;

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

    // CreateObject returns an ILTBaseClass*, not an HOBJECT.
    // The Spawner's MID_INITIALUPDATE registers its own m_hObject.
    ILTBaseClass *pController =
        g_pLTServer->CreateObject(
            hSpawnerClass,
            &ocs);

    if(!pController)
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

        if(s_nRoundAlive == 1 &&
           s_nRoundSpawned == s_nRoundTarget)
        {
            HCLASS hZombieClass =
                g_pLTServer->GetClass("FireteamZombie");
            for(HOBJECT h = g_pLTServer->GetNextObject(LTNULL); h;
                h = g_pLTServer->GetNextObject(h))
            {
                HCLASS hType = g_pLTServer->GetObjectClass(h);
                if(!hZombieClass || !hType ||
                   !g_pLTServer->IsKindOf(hType, hZombieClass))
                    continue;
                FireteamZombie *pZombie =
                    (FireteamZombie*)g_pLTServer->HandleToObject(h);
                if(pZombie && pZombie->IsAliveForRound())
                {
                    LTVector vPos;
                    g_pLTServer->GetObjectPos(h, &vPos);
                    g_pLTServer->CPrint(
                        "Fireteam final infected: last living zombie at %.1f %.1f %.1f; required kills %u/%u.",
                        vPos.x, vPos.y, vPos.z,
                        s_nRoundKilled, s_nRoundTarget);
                    break;
                }
            }
        }

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