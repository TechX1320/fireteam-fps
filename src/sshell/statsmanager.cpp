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

StatsManager::StatsManager():
m_iNumPlayers(0)
{
    ResetPlayerList();
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

bool StatsManager::SaveCompletedMatch(
    uint32 nRound, const char *pDifficulty, float fCombatSeconds)
{
    if(!GetNumPlayers())
        return false;

    // Works in BUILT for solo/host and BUILT/Dedicated for a headless host.
    // Folder-local history is never sent to any third-party API.
    _mkdir("data");
    _mkdir("data\\matches");

    const unsigned long nNow = (unsigned long)time(NULL);
    const unsigned long nTick = (unsigned long)clock();
    char sPath[260], sTemp[270];
    _snprintf(sPath, sizeof(sPath) - 1,
        "data\\matches\\match-%lu-%lu-%u.json",
        nNow, nTick, (unsigned)nRound);
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
    char sMatchId[128];
    _snprintf(sMatchId, sizeof(sMatchId) - 1,
        "match-%lu-%lu-%u", nNow, nTick, (unsigned)nRound);
    sMatchId[sizeof(sMatchId) - 1] = '\0';
    FT_WriteMatchJsonText(pFile, sMatchId);
    fprintf(pFile, ",\n\"endedAtUnix\":%lu,\n\"map\":", nNow);
    FT_WriteMatchJsonText(pFile, sMap);
    fprintf(pFile, ",\n\"difficulty\":");
    FT_WriteMatchJsonText(pFile, pDifficulty);
    fprintf(pFile,
        ",\n\"roundReached\":%u,\n\"combatSeconds\":%.2f,\n"
        "\"status\":\"completed\",\n\"players\":[\n",
        nRound, fCombatSeconds > 0.0f ? fCombatSeconds : 0.0f);

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
    if(rename(sTemp, sPath) != 0)
    {
        remove(sTemp);
        g_pLTServer->CPrint("Fireteam stats: failed finalizing %s.", sPath);
        return false;
    }

    g_pLTServer->CPrint(
        "Fireteam stats: match round %u saved to %s (local, unsigned).",
        nRound, sPath);
    return true;
}
