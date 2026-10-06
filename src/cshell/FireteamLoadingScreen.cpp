#include "FireteamLoadingScreen.h"
#include "clientinterfaces.h"

#include <windows.h>
#include <iltclient.h>
#include <iltfontmanager.h>
#include <stdio.h>
#include <string.h>

static HANDLE s_hStopEvent = NULL;
static HANDLE s_hThread = NULL;
static CUIFont *s_pLoadingFont = LTNULL;
static CUIFormattedPolyString *s_pLoadingText = LTNULL;
static char s_szLoadingText[256];

static void FT_RenderLoadingFrame()
{
    if(!g_pLTClient ||
       !s_pLoadingText)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    g_pLTClient->ClearScreen(
        LTNULL,
        CLEARSCREEN_SCREEN | CLEARSCREEN_RENDER,
        0);

    if(g_pLTClient->Start3D() != LT_OK)
    {
        return;
    }

    g_pLTClient->StartOptimized2D();

    const float fWidth = s_pLoadingText->GetWidth();
    const float fHeight = s_pLoadingText->GetHeight();

    s_pLoadingText->SetPosition(
        ((float)nScreenW - fWidth) * 0.5f,
        ((float)nScreenH - fHeight) * 0.5f);

    s_pLoadingText->Render();

    g_pLTClient->EndOptimized2D();
    g_pLTClient->End3D(END3D_CANDRAWCONSOLE);
    g_pLTClient->FlipScreen(0);
}

static DWORD WINAPI FT_LoadingThread(void *)
{
    while(s_hStopEvent &&
          WaitForSingleObject(
              s_hStopEvent,
              0) == WAIT_TIMEOUT)
    {
        FT_RenderLoadingFrame();

        // NOLF2's original loading thread deliberately ran at roughly 10fps,
        // leaving almost all CPU time to the synchronous world load.
        Sleep(100);
    }

    return 0;
}

void FT_LoadingScreenInit()
{
    if(!s_hStopEvent)
    {
        s_hStopEvent =
            CreateEvent(
                NULL,
                TRUE,
                FALSE,
                NULL);
    }

    if(s_pLoadingFont)
    {
        return;
    }

    s_pLoadingFont =
        g_pLTCFontManager->CreateFont(
            "fonts/SQR721B.TTF",
            "Square721 BT",
            28,
            46,
            255);

    if(!s_pLoadingFont)
    {
        return;
    }

    s_pLoadingFont->SetDefCharWidth(8);
    s_pLoadingFont->SetDefColor(0xFFFFFFFF);

    s_pLoadingText =
        g_pLTCFontManager->CreateFormattedPolyString(
            s_pLoadingFont,
            "FIRETEAM\n\nLOADING...");
}

void FT_LoadingScreenTerm()
{
    FT_LoadingScreenStop();

    if(s_pLoadingText)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pLoadingText);
        s_pLoadingText = LTNULL;
    }

    if(s_pLoadingFont)
    {
        g_pLTCFontManager->DestroyFont(
            s_pLoadingFont);
        s_pLoadingFont = LTNULL;
    }

    if(s_hStopEvent)
    {
        CloseHandle(s_hStopEvent);
        s_hStopEvent = NULL;
    }
}

bool FT_LoadingScreenStart(
    const char *pWorldName)
{
    FT_LoadingScreenInit();

    if(!s_hStopEvent ||
       !s_pLoadingText ||
       s_hThread)
    {
        return false;
    }

    const char *pDisplayWorld =
        (pWorldName && pWorldName[0])
        ? pWorldName
        : "WORLD";

    const char *pSlash =
        strrchr(pDisplayWorld, '/');
    if(!pSlash)
    {
        pSlash = strrchr(pDisplayWorld, '\\');
    }

    if(pSlash && pSlash[1])
    {
        pDisplayWorld = pSlash + 1;
    }

    sprintf(
        s_szLoadingText,
        "FIRETEAM\n\nLOADING %s...",
        pDisplayWorld);

    s_pLoadingText->SetText(
        s_szLoadingText);

    ResetEvent(s_hStopEvent);

    // Put something on screen immediately before the synchronous load begins.
    FT_RenderLoadingFrame();

    DWORD nThreadID = 0;
    s_hThread =
        CreateThread(
            NULL,
            0,
            FT_LoadingThread,
            NULL,
            0,
            &nThreadID);

    return s_hThread != NULL;
}

void FT_LoadingScreenStop()
{
    if(!s_hThread)
    {
        return;
    }

    if(s_hStopEvent)
    {
        SetEvent(s_hStopEvent);
    }

    WaitForSingleObject(
        s_hThread,
        INFINITE);

    CloseHandle(s_hThread);
    s_hThread = NULL;
}
