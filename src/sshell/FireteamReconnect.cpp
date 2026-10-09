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
    }
}

bool FT_ClaimReconnect(CPlayerSrvr *pPlayer, const char *pTicket)
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
    pUnused->bUsed = true;
    strncpy(pUnused->sTicket, pTicket, 32);
    pUnused->sTicket[32] = '\0';
    pUnused->pActive = pPlayer;
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
