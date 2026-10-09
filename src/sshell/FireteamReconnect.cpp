#include "FireteamReconnect.h"
#include "playersrvr.h"
#include "FireteamSpawner.h"
#include "serverinterfaces.h"

#include <string.h>

// Fixed, bounded, authoritative in-memory match store. No usernames or IPs
// used as identity. Server restarts/world switches reset these capabilities.
enum { FT_RESUME_CAPACITY = 128 };
struct FTResumeSlot
{
    bool bUsed;
    char sTicket[33];
    char sName[16];
    CPlayerSrvr *pActive;
    FTPlayerResumeState state;
};
static FTResumeSlot s_aResume[FT_RESUME_CAPACITY];

static bool FT_ValidReconnectTicket(const char *p)
{
    if(!p || strlen(p) != 32) return false;
    for(uint32 n = 0; n < 32; ++n)
    {
        const char c = p[n];
        if(!((c >= '0' && c <= '9') ||
             (c >= 'a' && c <= 'f') ||
             (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

void FT_ResetReconnect()
{
    for(uint32 n = 0; n < FT_RESUME_CAPACITY; ++n)
    {
        s_aResume[n].bUsed = false;
        s_aResume[n].pActive = LTNULL;
        s_aResume[n].sTicket[0] = '\0';
        s_aResume[n].sName[0] = '\0';
    }
}

// Names are friendly display labels, NOT identities. Keep one reserved
// per match-scoped reconnect ticket so Alt+F4 does not lose the label.
// ASCII-only bounded names avoid case-folding surprises in this legacy HUD.
static void FT_CleanPlayerName(const char *pRequested, char out[16])
{
    uint32 n = 0;
    if(pRequested)
    {
        for(uint32 i = 0; pRequested[i] && n < 15; ++i)
        {
            const unsigned char ch = (unsigned char)pRequested[i];
            if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
                out[n++] = (char)ch;
            else if(ch == ' ' && n > 0 && out[n-1] != ' ')
                out[n++] = ' ';
        }
    }
    while(n > 0 && out[n-1] == ' ') --n;
    out[n] = '\0';
    if(n == 0) strcpy(out, "Player");
}

static bool FT_NameReserved(const char *pName)
{
    for(uint32 i = 0; i < FT_RESUME_CAPACITY; ++i)
        if(s_aResume[i].bUsed && s_aResume[i].sName[0] &&
           _stricmp(pName, s_aResume[i].sName) == 0)
            return true;
    return false;
}

static bool FT_ChooseUniqueName(const char *pRequested, char result[16])
{
    char base[16];
    FT_CleanPlayerName(pRequested, base);
    if(!FT_NameReserved(base))
    {
        strcpy(result, base);
        return true;
    }
    for(uint32 suffix = 2; suffix <= FT_RESUME_CAPACITY + 2; ++suffix)
    {
        char tag[8];
        _snprintf(tag, sizeof(tag), "_%u", (unsigned)suffix);
        tag[sizeof(tag)-1] = '\0';
        const uint32 prefixLength = 15 - (uint32)strlen(tag);
        strncpy(result, base, prefixLength);
        result[prefixLength] = '\0';
        strcat(result, tag);
        if(!FT_NameReserved(result)) return true;
    }
    return false;
}

bool FT_ClaimReconnect(CPlayerSrvr *pPlayer, const char *pTicket,
                       const char *pRequestedName)
{
    if(!pPlayer || pPlayer->HasResumeIdentity() ||
       !FT_ValidReconnectTicket(pTicket))
        return false;

    FTResumeSlot *pUnused = LTNULL;
    for(uint32 n = 0; n < FT_RESUME_CAPACITY; ++n)
    {
        FTResumeSlot &slot = s_aResume[n];
        if(!slot.bUsed)
        {
            if(!pUnused) pUnused = &slot;
            continue;
        }
        if(_stricmp(slot.sTicket, pTicket) != 0) continue;

        if(slot.pActive)
        {
            // Never let a cloned token hijack an online player.
            g_pLTServer->CPrint(
                "FIRETEAM reconnect: identity already online, rejected.");
            return false;
        }

        slot.pActive = pPlayer;
        pPlayer->SetPlayerName(slot.sName);
        pPlayer->RestoreResumeState(pTicket, slot.state);
        g_pLTServer->CPrint(
            "FIRETEAM reconnect: recovered HP/lives/stats/position.");
        return true;
    }

    if(!pUnused)
    {
        g_pLTServer->CPrint(
            "FIRETEAM reconnect: identity capacity reached.");
        return false;
    }
    char acceptedName[16];
    if(!FT_ChooseUniqueName(pRequestedName, acceptedName))
        return false;
    pUnused->bUsed = true;
    strcpy(pUnused->sName, acceptedName);
    strncpy(pUnused->sTicket, pTicket, 32);
    pUnused->sTicket[32] = '\0';
    pUnused->pActive = pPlayer;
    pPlayer->SetPlayerName(acceptedName);
    // Fresh (including deliberately regenerated) tickets cannot spawn into
    // an already-running wave and steal a full-health replacement life.
    const bool bWait = FT_GetFireteamCurrentRound() > 0 ||
                       FT_IsFireteamMatchOver();
    pPlayer->AuthorizeNewSession(pTicket, bWait);
    g_pLTServer->CPrint("FIRETEAM reconnect: new player %s.",
        bWait ? "waiting until next round" : "ready before round 1");
    return true;
}

void FT_ParkReconnect(CPlayerSrvr *pPlayer)
{
    if(!pPlayer || !pPlayer->HasResumeIdentity()) return;
    for(uint32 n = 0; n < FT_RESUME_CAPACITY; ++n)
    {
        FTResumeSlot &slot = s_aResume[n];
        if(slot.bUsed && slot.pActive == pPlayer)
        {
            pPlayer->CaptureResumeState(slot.state);
            slot.pActive = LTNULL;
            return;
        }
    }
}
