//------------------------------------------------------------------------------//
//
// MODULE	: StatsManager.cpp
//
// PURPOSE	: StatsManager - Implementation
//
// CREATED	: 07/17/2003
//
// (c) 2003 Touchdown Entertainment / LithTech, Inc.	All Rights Reserved
//
//------------------------------------------------------------------------------//

#include "statsmanager.h"
#include "playersrvr.h"
#include "scoredefs.h"
#include "serverinterfaces.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <windows.h>

StatsManager::StatsManager():
m_iNumPlayers(0),
m_bMatchFinalized(false)
{
    m_sMatchId[0] = '\0';
    ResetPlayerList();
    BeginMatch();
}


StatsManager::~StatsManager()
{
    ResetPlayerList();
}


void StatsManager::RegisterPlayer(CPlayerSrvr *pPlayer)
{
    m_pPlayers.Append(pPlayer);
}

void StatsManager::RemovePlayer(CPlayerSrvr *_pPlayer)
{
    // Find the LinkedMember which contains the player that we want to remove.

    LinkedMember<CPlayerSrvr*> *pMember = m_pPlayers.First();

    for(int i = 0; pMember; i++)
    {
        // Get handle to the server player object data in the LinkedMember
        CPlayerSrvr *pPlayer = pMember->data;

        if(_pPlayer == pPlayer)
        {

            m_pPlayers.Remove(pMember);
            return;
        }

        // Next element
        pMember = pMember->next;
    }

}

void StatsManager::ResetPlayerList()
{
    // Zero out the player list.
    m_pPlayers.FreeStack();
}

uint32 StatsManager::GetNumPlayers()
{
    return m_pPlayers.GetSize();
}

LTRESULT StatsManager::GetPlayerScores(SCORESTRUCT *scores)
{

    LinkedMember<CPlayerSrvr*> *pMember = m_pPlayers.First();

    uint32 iSize = GetNumPlayers();
    for(uint32 i = 0; i < iSize; ++i)
    {
        if(!pMember)
        {
            break;
        }

        // Get handle to the server player object data in the LinkedMember
        CPlayerSrvr *pPlayer = pMember->data;

        // Set the struct info
        scores[i].iClientID = pPlayer->GetClientID();
        sprintf(scores[i].sPlayerName, "%s", pPlayer->GetPlayerName());
        scores[i].iScore    = pPlayer->GetScore();
        scores[i].iLives    = pPlayer->GetLives();
        scores[i].fMoney    = pPlayer->GetMoney();
        scores[i].iShotsFired = pPlayer->GetAcceptedShots();
        scores[i].iShotsHit = pPlayer->GetConfirmedHits();
        scores[i].iDeaths = pPlayer->GetMatchDeaths();
        scores[i].iPowerups = pPlayer->GetPowerupCount();
        scores[i].iHeadshotKills = pPlayer->GetHeadshotKills();

        // Next element
        pMember = pMember->next;
    }

    return LT_OK;
}


// JSON escaping is compulsory: a player can choose a name containing quotes.
// Names only appear INSIDE JSON; filenames use timestamp/clock, never names.
static void FT_WriteMatchJsonText(FILE *pFile, const char *pText)
{
    fputc('"', pFile);
    const unsigned char *p = (const unsigned char*)(pText ? pText : "");
    for(; *p; ++p)
    {
        if(*p == '"' || *p == '\\')
        {
            fputc('\\', pFile);
            fputc(*p, pFile);
        }
        else if(*p < 0x20)
        {
            fprintf(pFile, "\\u%04x", (unsigned)*p);
        }
        else
        {
            fputc(*p, pFile);
        }
    }
    fputc('"', pFile);
}

static void FT_WriteNamedMatchJson(
    FILE *pFile, const FTNamedCounter *pValues, uint8 nCount)
{
    fputc('{', pFile);
    for(uint8 n = 0; n < nCount; ++n)
    {
        if(n) fputc(',', pFile);
        FT_WriteMatchJsonText(pFile, pValues[n].sId);
        fprintf(pFile, ":%u", pValues[n].nCount);
    }
    fputc('}', pFile);
}

void StatsManager::BeginMatch()
{
    // Called whenever a new world controller is created; the ID stays fixed
    // through all checkpoints AND the final Game Over write.
    static uint32 s_nMatchSequence = 0;
    ++s_nMatchSequence;
    _snprintf(m_sMatchId, sizeof(m_sMatchId) - 1,
        "match-%lu-%lu-%u",
        (unsigned long)time(NULL),
        (unsigned long)GetTickCount(),
        (unsigned)s_nMatchSequence);
    m_sMatchId[sizeof(m_sMatchId) - 1] = '\0';
    m_bMatchFinalized = false;
}

bool StatsManager::SaveMatchSnapshot(
    uint32 nRound, const char *pDifficulty, float fCombatSeconds,
    bool bCompleted)
{
    if(!GetNumPlayers() || nRound == 0)
        return false;
    if(m_bMatchFinalized)
        return false;
    if(!m_sMatchId[0])
        BeginMatch();

    // Works in BUILT for solo/host and BUILT/Dedicated for a headless host.
    // Folder-local history is never sent to any third-party API.
    _mkdir("data");
    _mkdir("data\\matches");

    const unsigned long nNow = (unsigned long)time(NULL);
    char sPath[260], sTemp[270];
    _snprintf(sPath, sizeof(sPath) - 1,
        "data\\matches\\%s.json", m_sMatchId);
    sPath[sizeof(sPath) - 1] = '\0';
    _snprintf(sTemp, sizeof(sTemp) - 1, "%s.tmp", sPath);
    sTemp[sizeof(sTemp) - 1] = '\0';

    FILE *pFile = fopen(sTemp, "wb");
    if(!pFile)
    {
        g_pLTServer->CPrint(
            "Fireteam stats: cannot create match snapshot at %s.", sTemp);
        return false;
    }

    char sMap[96] = "unknown";
    FILE *pSession = fopen("config/session.cfg", "rt");
    if(pSession)
    {
        char sLine[256];
        while(fgets(sLine, sizeof(sLine), pSession))
        {
            char sValue[96] = {0};
            if(sscanf(sLine, "map=%95[A-Za-z0-9_-]", sValue) == 1)
            {
                strncpy(sMap, sValue, sizeof(sMap) - 1);
                sMap[sizeof(sMap) - 1] = '\0';
                break;
            }
        }
        fclose(pSession);
    }

    fprintf(pFile,
        "{\n\"schemaVersion\":1,\n\"source\":\"local-unverified\",\n"
        "\"matchId\":");
    FT_WriteMatchJsonText(pFile, m_sMatchId);
    fprintf(pFile,
        ",\n\"lastSavedAtUnix\":%lu,\n\"endedAtUnix\":%lu,\n\"map\":",
        nNow, bCompleted ? nNow : 0ul);
    FT_WriteMatchJsonText(pFile, sMap);
    fprintf(pFile, ",\n\"difficulty\":");
    FT_WriteMatchJsonText(pFile, pDifficulty);
    fprintf(pFile,
        ",\n\"roundReached\":%u,\n\"combatSeconds\":%.2f,\n"
        "\"status\":\"%s\",\n\"players\":[\n",
        nRound, fCombatSeconds > 0.0f ? fCombatSeconds : 0.0f,
        bCompleted ? "completed" : "in_progress");

    LinkedMember<CPlayerSrvr*> *pMember = m_pPlayers.First();
    bool bFirst = true;
    for(; pMember; pMember = pMember->next)
    {
        CPlayerSrvr *pPlayer = pMember->data;
        if(!pPlayer) continue;
        if(!bFirst) fprintf(pFile, ",\n");
        bFirst = false;

        fprintf(pFile, "{\"name\":");
        FT_WriteMatchJsonText(pFile, pPlayer->GetPlayerName());
        fprintf(pFile,
            ",\"clientId\":%u,\"killsTotal\":%u,"
            "\"shotsFired\":%u,\"hitscanHits\":%u,"
            "\"headshotKills\":%u,\"deaths\":%u,"
            "\"powerups\":%u,\"damageTaken\":%u,"
            "\"livesRemaining\":%u,\"killsByType\":",
            pPlayer->GetClientID(),
            pPlayer->GetScore(),
            pPlayer->GetAcceptedShots(),
            pPlayer->GetConfirmedHits(),
            pPlayer->GetHeadshotKills(),
            pPlayer->GetMatchDeaths(),
            pPlayer->GetPowerupCount(),
            pPlayer->GetDamageTaken(),
            (unsigned)pPlayer->GetLives());
        FT_WriteNamedMatchJson(
            pFile, pPlayer->GetZombieKillTypes(),
            pPlayer->GetZombieTypeCount());

        fprintf(pFile, ",\"powerupsById\":");
        FT_WriteNamedMatchJson(
            pFile, pPlayer->GetPowerupTypes(),
            pPlayer->GetPowerupTypeCount());

        fprintf(pFile, ",\"shotsByWeapon\":[");
        bool bFirstWeapon = true;
        for(uint8 nSlot = 1; nSlot <= 5; ++nSlot)
        {
            const char *pWeaponId = pPlayer->GetWeaponId(nSlot);
            if(!pWeaponId || !pWeaponId[0]) continue;
            if(!bFirstWeapon) fputc(',', pFile);
            bFirstWeapon = false;
            fprintf(pFile, "{\"id\":");
            FT_WriteMatchJsonText(pFile, pWeaponId);
            fprintf(pFile, ",\"slot\":%u,\"shots\":%u}",
                (unsigned)nSlot, pPlayer->GetWeaponShots(nSlot));
        }
        fprintf(pFile, "]}");
    }
    fprintf(pFile, "\n]}\n");

    const bool bSaved = ferror(pFile) == 0 && fflush(pFile) == 0;
    if(fclose(pFile) != 0 || !bSaved)
    {
        remove(sTemp);
        g_pLTServer->CPrint("Fireteam stats: failed writing %s.", sTemp);
        return false;
    }
    // Win32 MoveFileEx atomically replaces the prior checkpoint, unlike
    // CRT rename() which fails when the destination already exists.
    if(!MoveFileExA(sTemp, sPath,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const DWORD nError = GetLastError();
        remove(sTemp);
        g_pLTServer->CPrint(
            "Fireteam stats: unable to replace %s (Win32 error %lu).",
            sPath, (unsigned long)nError);
        return false;
    }

    if(bCompleted)
        m_bMatchFinalized = true;
    g_pLTServer->CPrint(
        "Fireteam stats: %s round %u -> %s (local, unsigned).",
        bCompleted ? "FINAL" : "checkpoint", nRound, sPath);
    return true;
}
