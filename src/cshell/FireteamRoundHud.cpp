#include "FireteamRoundHud.h"
#include "FireteamCombatFeedback.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltfontmanager.h>
#include <iltmessage.h>
#include <iltsoundmgr.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

static CUIFont *s_pRoundFont = LTNULL;
static CUIFont *s_pBuffFont = LTNULL;
static CUIFormattedPolyString *s_pAnnouncement = LTNULL;
static CUIFormattedPolyString *s_pRoundStatus = LTNULL;
static CUIFormattedPolyString *s_pBottomlessStatus = LTNULL;
static CUIFormattedPolyString *s_pOneHitStatus = LTNULL;
static CUIFormattedPolyString *s_pGodStatus = LTNULL;
static CUIFormattedPolyString *s_pWallhackStatus = LTNULL;
static CUIFormattedPolyString *s_pSpectatorStatus = LTNULL;
static CUIFormattedPolyString *s_pRespawnStatus = LTNULL;

static uint16 s_nRound = 0;
static uint16 s_nTarget = 0;
static uint16 s_nKilled = 0;
static uint16 s_nAlive = 0;
static uint8 s_nLives = 3;
static uint8 s_nMaxLives = 3;
static bool s_bGameOver = false;
static bool s_bSpectating = false;
static bool s_bQaSpectating = false;
static float s_fAnnouncementUntil = 0.0f;
static float s_fRespawnUntil = 0.0f;
static float s_fBottomlessUntil = 0.0f;
static float s_fOneHitUntil = 0.0f;
static float s_fGodUntil = 0.0f;
static float s_fWallhackUntil = 0.0f;

static void FT_PlayRoundCue(
    const char *pSound)
{
    if(!g_pLTCSoundMgr ||
       !pSound ||
       !pSound[0])
    {
        return;
    }

    PlaySoundInfo soundInfo;
    PLAYSOUNDINFO_INIT(
        soundInfo);

    soundInfo.m_dwFlags =
        PLAYSOUND_LOCAL |
        PLAYSOUND_CTRL_VOL;
    soundInfo.m_nVolume =
        74;

    strncpy(
        soundInfo.m_szSoundName,
        pSound,
        sizeof(soundInfo.m_szSoundName) - 1);
    soundInfo.m_szSoundName[
        sizeof(soundInfo.m_szSoundName) - 1] =
        '\0';

    HLTSOUND hSound =
        LTNULL;

    g_pLTCSoundMgr->PlaySound(
        &soundInfo,
        hSound);
}

void FT_RoundHudInit()
{
    if(s_pRoundFont)
    {
        return;
    }

    s_pRoundFont = g_pLTCFontManager->CreateFont(
        "fonts/SQR721B.TTF",
        "Square721 BT",
        26,
        42,
        255);

    if(!s_pRoundFont)
    {
        return;
    }

    s_pRoundFont->SetDefCharWidth(7);
    s_pRoundFont->SetDefColor(0xFFFFFFFF);

    s_pBuffFont = g_pLTCFontManager->CreateFont(
        "fonts/SQR721B.TTF",
        "Square721 BT",
        18,
        30,
        255);

    if(s_pBuffFont)
    {
        s_pBuffFont->SetDefCharWidth(6);
        s_pBuffFont->SetDefColor(0xFFFFFFFF);
    }

    s_pAnnouncement =
        g_pLTCFontManager->CreateFormattedPolyString(
            s_pRoundFont,
            "");

    s_pRoundStatus =
        g_pLTCFontManager->CreateFormattedPolyString(
            s_pRoundFont,
            "");

    if(s_pBuffFont)
    {
        s_pBottomlessStatus =
            g_pLTCFontManager->CreateFormattedPolyString(
                s_pBuffFont,
                "");

        s_pOneHitStatus =
            g_pLTCFontManager->CreateFormattedPolyString(
                s_pBuffFont,
                "");

        s_pGodStatus =
            g_pLTCFontManager->CreateFormattedPolyString(
                s_pBuffFont,
                "");

        s_pWallhackStatus =
            g_pLTCFontManager->CreateFormattedPolyString(
                s_pBuffFont,
                "");

        s_pSpectatorStatus =
            g_pLTCFontManager->CreateFormattedPolyString(
                s_pBuffFont,
                "");

        s_pRespawnStatus =
            g_pLTCFontManager->CreateFormattedPolyString(
                s_pBuffFont,
                "");
    }

    if(s_pRespawnStatus)
        s_pRespawnStatus->SetColor(0xFFFFFFFF);

    if(s_pAnnouncement)
        s_pAnnouncement->SetColor(0xFFFFB000);

    if(s_pRoundStatus)
        s_pRoundStatus->SetColor(0xFFFFFFFF);

    if(s_pBottomlessStatus)
        s_pBottomlessStatus->SetColor(0xFF30BDE7);

    if(s_pOneHitStatus)
        s_pOneHitStatus->SetColor(0xFFFFB000);

    if(s_pGodStatus)
        s_pGodStatus->SetColor(0xFFFFF06A);

    if(s_pWallhackStatus)
        s_pWallhackStatus->SetColor(0xFFFF5AE0);

    if(s_pSpectatorStatus)
        s_pSpectatorStatus->SetColor(0xFF30BDE7);
}

void FT_RoundHudTerm()
{
    if(s_pRespawnStatus)
    {
        g_pLTCFontManager->DestroyPolyString(s_pRespawnStatus);
        s_pRespawnStatus = LTNULL;
    }

    if(s_pAnnouncement)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pAnnouncement);
        s_pAnnouncement = LTNULL;
    }

    if(s_pRoundStatus)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pRoundStatus);
        s_pRoundStatus = LTNULL;
    }

    if(s_pBottomlessStatus)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pBottomlessStatus);
        s_pBottomlessStatus = LTNULL;
    }

    if(s_pOneHitStatus)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pOneHitStatus);
        s_pOneHitStatus = LTNULL;
    }

    if(s_pGodStatus)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pGodStatus);
        s_pGodStatus = LTNULL;
    }

    if(s_pWallhackStatus)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pWallhackStatus);
        s_pWallhackStatus = LTNULL;
    }

    if(s_pSpectatorStatus)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pSpectatorStatus);
        s_pSpectatorStatus = LTNULL;
    }

    if(s_pBuffFont)
    {
        g_pLTCFontManager->DestroyFont(
            s_pBuffFont);
        s_pBuffFont = LTNULL;
    }

    if(s_pRoundFont)
    {
        g_pLTCFontManager->DestroyFont(
            s_pRoundFont);
        s_pRoundFont = LTNULL;
    }

    s_nRound = 0;
    s_nTarget = 0;
    s_nKilled = 0;
    s_nAlive = 0;
    s_nLives = 3;
    s_nMaxLives = 3;
    s_bGameOver = false;
    s_bSpectating = false;
    s_bQaSpectating = false;
    s_fAnnouncementUntil = 0.0f;
    s_fRespawnUntil = 0.0f;
    s_fBottomlessUntil = 0.0f;
    s_fOneHitUntil = 0.0f;
    s_fGodUntil = 0.0f;
    s_fWallhackUntil = 0.0f;
}

void FT_RoundHudHandleMessage(
    ILTMessage_Read *pMessage)
{
    if(!pMessage)
    {
        return;
    }

    const uint8 nState = pMessage->Readuint8();
    s_nRound = pMessage->Readuint16();
    s_nTarget = pMessage->Readuint16();
    s_nKilled = pMessage->Readuint16();
    s_nAlive = pMessage->Readuint16();

    if(!s_pRoundFont)
    {
        FT_RoundHudInit();
    }

    if(!s_pAnnouncement)
    {
        return;
    }

    char szAnnouncement[96];

    if(nState == 1)
    {
        s_bGameOver = false;

        sprintf(
            szAnnouncement,
            "ROUND %u  BEGIN",
            (uint32)s_nRound);
        s_pAnnouncement->SetText(szAnnouncement);
        s_fAnnouncementUntil =
            g_pLTClient->GetTime() + 3.0f;

        // Original Cabin Fever section sting + original CA round-start art.
        FT_PlayRoundCue(
            "Snd/CABINFEVER/SECTION1.WAV");
        FT_CombatFeedbackShowRoundStart();
    }
    else if(nState == 2)
    {
        sprintf(
            szAnnouncement,
            "ROUND %u  CLEAR",
            (uint32)s_nRound);
        s_pAnnouncement->SetText(szAnnouncement);
        s_fAnnouncementUntil =
            g_pLTClient->GetTime() + 3.0f;

        // Companion Cabin Fever section sting for a cleared round.
        FT_PlayRoundCue(
            "Snd/CABINFEVER/SECTION2.WAV");
    }
    else if(nState == 3)
    {
        s_bGameOver = true;
        s_pAnnouncement->SetText(
            "GAME OVER");
        s_fAnnouncementUntil =
            FLT_MAX;
    }
}

void FT_RoundHudShowAnnouncement(
    const char *pText,
    float fSeconds)
{
    if(!pText || !pText[0])
    {
        return;
    }

    if(!s_pRoundFont)
    {
        FT_RoundHudInit();
    }

    if(!s_pAnnouncement)
    {
        return;
    }

    s_pAnnouncement->SetText(
        pText);

    s_fAnnouncementUntil =
        g_pLTClient->GetTime() +
        (fSeconds > 0.0f
            ? fSeconds
            : 3.0f);
}

void FT_RoundHudSetTimedPowerups(
    float fBottomlessSeconds,
    float fOneHitSeconds,
    float fGodSeconds,
    float fWallhackSeconds)
{
    if(!s_pRoundFont)
    {
        FT_RoundHudInit();
    }

    const float fNow =
        g_pLTClient->GetTime();

    s_fBottomlessUntil =
        fBottomlessSeconds > 0.0f
        ? fNow + fBottomlessSeconds
        : 0.0f;

    s_fOneHitUntil =
        fOneHitSeconds > 0.0f
        ? fNow + fOneHitSeconds
        : 0.0f;

    s_fGodUntil =
        fGodSeconds > 0.0f
        ? fNow + fGodSeconds
        : 0.0f;

    s_fWallhackUntil =
        fWallhackSeconds > 0.0f
        ? fNow + fWallhackSeconds
        : 0.0f;
}

void FT_RoundHudSetRespawnCountdown(float fSeconds)
{
    s_fRespawnUntil = fSeconds > 0.0f
        ? g_pLTClient->GetTime() + fSeconds
        : 0.0f;
}

void FT_RoundHudSetLives(
    uint8 nLives,
    uint8 nMaxLives)
{
    if(!s_pRoundFont)
    {
        FT_RoundHudInit();
    }

    const uint8 nPreviousLives =
        s_nLives;

    s_nLives = nLives;
    s_nMaxLives =
        nMaxLives > 0
        ? nMaxLives
        : 1;

    if(nPreviousLives > 0 &&
       s_nLives == 0 &&
       !s_bGameOver &&
       s_pAnnouncement)
    {
        s_pAnnouncement->SetText(
            "OUT OF LIVES");
        s_fAnnouncementUntil =
            g_pLTClient->GetTime() +
            4.0f;
    }
}

bool FT_RoundHudIsPlayerEliminated()
{
    return s_nLives == 0;
}

bool FT_RoundHudIsGameOver()
{
    return s_bGameOver;
}

void FT_RoundHudSetSpectator(
    bool bSpectating,
    bool bQaMode)
{
    s_bSpectating =
        bSpectating;
    s_bQaSpectating =
        bSpectating &&
        bQaMode;
}

void FT_RenderRoundHud()
{
    if(!s_pRoundFont)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    if(s_pRoundStatus &&
       s_nRound > 0)
    {
        char szStatus[96];
        sprintf(
            szStatus,
            "ROUND %u     KILLS %u/%u     ALIVE %u     LIVES %u/%u",
            (uint32)s_nRound,
            (uint32)s_nKilled,
            (uint32)s_nTarget,
            (uint32)s_nAlive,
            (uint32)s_nLives,
            (uint32)s_nMaxLives);

        s_pRoundStatus->SetText(szStatus);

        const float fWidth =
            s_pRoundStatus->GetWidth();

        s_pRoundStatus->SetPosition(
            ((float)nScreenW - fWidth) * 0.5f,
            22.0f);

        s_pRoundStatus->Render();
    }

    const float fRespawnRemaining = s_fRespawnUntil - g_pLTClient->GetTime();
    if(s_pRespawnStatus && fRespawnRemaining > 0.0f && s_nLives > 0 && !s_bGameOver)
    {
        char szRespawn[128];
        sprintf(szRespawn, "YOU DIED  |  RESPAWN IN %u  |  ESC: QUIT / SETTINGS", (uint32)(fRespawnRemaining + 0.999f));
        s_pRespawnStatus->SetText(szRespawn);
        s_pRespawnStatus->SetPosition(
            ((float)nScreenW - s_pRespawnStatus->GetWidth()) * 0.5f,
            (float)nScreenH * 0.48f);
        s_pRespawnStatus->Render();
    }

    if(s_pSpectatorStatus &&
       s_bSpectating)
    {
        s_pSpectatorStatus->SetText(
            s_bQaSpectating
                ? "QA FREECAM  |  WASD MOVE  SPACE/CTRL UP/DOWN  SHIFT FAST  F8 EXIT"
                : "SPECTATOR FREECAM  |  WASD MOVE  SPACE/CTRL UP/DOWN  SHIFT FAST");

        const float fWidth =
            s_pSpectatorStatus->GetWidth();

        s_pSpectatorStatus->SetPosition(
            ((float)nScreenW - fWidth) * 0.5f,
            56.0f);

        s_pSpectatorStatus->Render();
    }

    const float fNow =
        g_pLTClient->GetTime();

    float fBuffY =
        (float)nScreenH - 136.0f;

    if(s_pBottomlessStatus &&
       fNow < s_fBottomlessUntil)
    {
        const float fRemaining =
            s_fBottomlessUntil - fNow;

        char szBuff[64];
        sprintf(
            szBuff,
            "BOTTOMLESS MAG  %us",
            (uint32)(fRemaining + 0.999f));

        s_pBottomlessStatus->SetText(
            szBuff);

        s_pBottomlessStatus->SetPosition(
            (float)nScreenW -
                s_pBottomlessStatus->GetWidth() -
                34.0f,
            fBuffY);

        s_pBottomlessStatus->Render();
        fBuffY -= 28.0f;
    }

    if(s_pOneHitStatus &&
       fNow < s_fOneHitUntil)
    {
        const float fRemaining =
            s_fOneHitUntil - fNow;

        char szBuff[64];
        sprintf(
            szBuff,
            "ONE HIT KILL  %us",
            (uint32)(fRemaining + 0.999f));

        s_pOneHitStatus->SetText(
            szBuff);

        s_pOneHitStatus->SetPosition(
            (float)nScreenW -
                s_pOneHitStatus->GetWidth() -
                34.0f,
            fBuffY);

        s_pOneHitStatus->Render();
        fBuffY -= 28.0f;
    }

    if(s_pGodStatus &&
       fNow < s_fGodUntil)
    {
        const float fRemaining =
            s_fGodUntil - fNow;

        char szBuff[64];
        sprintf(
            szBuff,
            "GOD MODE  %us",
            (uint32)(fRemaining + 0.999f));

        s_pGodStatus->SetText(
            szBuff);

        s_pGodStatus->SetPosition(
            (float)nScreenW -
                s_pGodStatus->GetWidth() -
                34.0f,
            fBuffY);

        s_pGodStatus->Render();
        fBuffY -= 28.0f;
    }

    if(s_pWallhackStatus &&
       fNow < s_fWallhackUntil)
    {
        const float fRemaining =
            s_fWallhackUntil - fNow;

        char szBuff[64];
        sprintf(
            szBuff,
            "ZOMBIE WALLHACK  %us",
            (uint32)(fRemaining + 0.999f));

        s_pWallhackStatus->SetText(
            szBuff);

        s_pWallhackStatus->SetPosition(
            (float)nScreenW -
                s_pWallhackStatus->GetWidth() -
                34.0f,
            fBuffY);

        s_pWallhackStatus->Render();
    }

    if(s_pAnnouncement &&
       fNow <
            s_fAnnouncementUntil)
    {
        const float fWidth =
            s_pAnnouncement->GetWidth();
        const float fHeight =
            s_pAnnouncement->GetHeight();

        s_pAnnouncement->SetPosition(
            ((float)nScreenW - fWidth) * 0.5f,
            ((float)nScreenH - fHeight) * 0.34f);

        s_pAnnouncement->Render();
    }
}
