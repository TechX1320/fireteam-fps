#include "FireteamRoundHud.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltfontmanager.h>
#include <iltmessage.h>
#include <stdio.h>

static CUIFont *s_pRoundFont = LTNULL;
static CUIFormattedPolyString *s_pAnnouncement = LTNULL;
static CUIFormattedPolyString *s_pRoundStatus = LTNULL;

static uint16 s_nRound = 0;
static uint16 s_nTarget = 0;
static uint16 s_nKilled = 0;
static uint16 s_nAlive = 0;
static float s_fAnnouncementUntil = 0.0f;

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

    s_pAnnouncement =
        g_pLTCFontManager->CreateFormattedPolyString(
            s_pRoundFont,
            "");

    s_pRoundStatus =
        g_pLTCFontManager->CreateFormattedPolyString(
            s_pRoundFont,
            "");

    if(s_pAnnouncement)
        s_pAnnouncement->SetColor(0xFFFFB000);

    if(s_pRoundStatus)
        s_pRoundStatus->SetColor(0xFFFFFFFF);
}

void FT_RoundHudTerm()
{
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
    s_fAnnouncementUntil = 0.0f;
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
        sprintf(
            szAnnouncement,
            "ROUND %u  BEGIN",
            (uint32)s_nRound);
        s_pAnnouncement->SetText(szAnnouncement);
        s_fAnnouncementUntil =
            g_pLTClient->GetTime() + 3.0f;
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

void FT_RenderRoundHud()
{
    if(!s_pRoundFont || s_nRound == 0)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    if(s_pRoundStatus)
    {
        char szStatus[96];
        sprintf(
            szStatus,
            "ROUND %u     KILLS %u/%u     ALIVE %u",
            (uint32)s_nRound,
            (uint32)s_nKilled,
            (uint32)s_nTarget,
            (uint32)s_nAlive);

        s_pRoundStatus->SetText(szStatus);

        const float fWidth =
            s_pRoundStatus->GetWidth();

        s_pRoundStatus->SetPosition(
            ((float)nScreenW - fWidth) * 0.5f,
            22.0f);

        s_pRoundStatus->Render();
    }

    if(s_pAnnouncement &&
       g_pLTClient->GetTime() <
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
