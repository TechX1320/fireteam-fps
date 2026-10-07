#include "FireteamLoadingScreen.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltfontmanager.h>
#include <stdio.h>
#include <string.h>

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
        CLEARSCREEN_SCREEN |
        CLEARSCREEN_RENDER,
        0);

    if(g_pLTClient->Start3D() != LT_OK)
    {
        return;
    }

    g_pLTClient->StartOptimized2D();

    const float fWidth =
        s_pLoadingText->GetWidth();

    const float fHeight =
        s_pLoadingText->GetHeight();

    s_pLoadingText->SetPosition(
        ((float)nScreenW - fWidth) * 0.5f,
        ((float)nScreenH - fHeight) * 0.5f);

    s_pLoadingText->Render();

    g_pLTClient->EndOptimized2D();
    g_pLTClient->End3D(
        END3D_CANDRAWCONSOLE);

    g_pLTClient->FlipScreen(0);
}

void FT_LoadingScreenInit()
{
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
    s_pLoadingFont->SetDefColor(
        0xFFFFFFFF);

    s_pLoadingText =
        g_pLTCFontManager->
            CreateFormattedPolyString(
                s_pLoadingFont,
                "FIRETEAM\n\nLOADING...");
}

void FT_LoadingScreenTerm()
{
    if(s_pLoadingText)
    {
        g_pLTCFontManager->
            DestroyPolyString(
                s_pLoadingText);

        s_pLoadingText = LTNULL;
    }

    if(s_pLoadingFont)
    {
        g_pLTCFontManager->
            DestroyFont(
                s_pLoadingFont);

        s_pLoadingFont = LTNULL;
    }
}

bool FT_LoadingScreenStart(
    const char *pWorldName)
{
    FT_LoadingScreenInit();

    if(!s_pLoadingText)
    {
        return false;
    }

    const char *pDisplayWorld =
        (pWorldName &&
         pWorldName[0])
        ? pWorldName
        : "WORLD";

    const char *pSlash =
        strrchr(
            pDisplayWorld,
            '/');

    if(!pSlash)
    {
        pSlash =
            strrchr(
                pDisplayWorld,
                '\\');
    }

    if(pSlash && pSlash[1])
    {
        pDisplayWorld =
            pSlash + 1;
    }

    sprintf(
        s_szLoadingText,
        "FIRETEAM\n\nLOADING %s...",
        pDisplayWorld);

    s_pLoadingText->SetText(
        s_szLoadingText);

    // Render once on the engine/client thread before the synchronous world
    // load. The previous experimental background render thread called the
    // Jupiter renderer concurrently with loading. That is unsafe when the
    // window loses focus/changes device state and could corrupt startup state.
    //
    // A single committed frame is less flashy, but deterministic. The external
    // launcher can own richer loading/presentation without racing LithTech.
    FT_RenderLoadingFrame();

    return true;
}

void FT_LoadingScreenStop()
{
    // Deliberately no worker thread to stop. The next normal game frame
    // replaces the committed loading frame.
}
