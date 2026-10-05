#include "FireteamSettingsMenu.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltdrawprim.h>
#include <iltfontmanager.h>
#include <stdio.h>
#include <windows.h>

struct FTResolution
{
    uint32 nWidth;
    uint32 nHeight;
};

static const FTResolution s_aResolutions[] =
{
    { 1024,  768 },
    { 1280,  720 },
    { 1366,  768 },
    { 1600,  900 },
    { 1920, 1080 },
    { 2560, 1440 },
    { 3440, 1440 },
    { 3840, 2160 }
};

static const uint32 s_nResolutionCount =
    sizeof(s_aResolutions) / sizeof(s_aResolutions[0]);

static CUIFont *s_pFont = LTNULL;
static CUIFormattedPolyString *s_pTitle = LTNULL;
static CUIFormattedPolyString *s_pBody = LTNULL;

static bool s_bOpen = false;
static bool s_bWindowed = true;
static uint32 s_nResolution = 1;
static uint32 s_nSelected = 0;
static float s_fSensitivity = 0.004625f;

static float FTClamp(float fValue, float fMin, float fMax)
{
    if(fValue < fMin) return fMin;
    if(fValue > fMax) return fMax;
    return fValue;
}

static float FTGetConsoleFloat(const char *pszName, float fDefault)
{
    HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar(pszName);
    return hVar ? g_pLTClient->GetVarValueFloat(hVar) : fDefault;
}

static void FTReadCurrentVideo()
{
    uint32 nWidth = (uint32)FTGetConsoleFloat("ScreenWidth", 1280.0f);
    uint32 nHeight = (uint32)FTGetConsoleFloat("ScreenHeight", 720.0f);
    s_bWindowed = FTGetConsoleFloat("Windowed", 1.0f) != 0.0f;

    uint32 nBest = 0;
    uint32 nBestDelta = 0xFFFFFFFF;

    for(uint32 i = 0; i < s_nResolutionCount; ++i)
    {
        uint32 dx = (s_aResolutions[i].nWidth > nWidth) ?
            (s_aResolutions[i].nWidth - nWidth) :
            (nWidth - s_aResolutions[i].nWidth);

        uint32 dy = (s_aResolutions[i].nHeight > nHeight) ?
            (s_aResolutions[i].nHeight - nHeight) :
            (nHeight - s_aResolutions[i].nHeight);

        uint32 nDelta = dx + dy;
        if(nDelta < nBestDelta)
        {
            nBestDelta = nDelta;
            nBest = i;
        }
    }

    s_nResolution = nBest;
}

static void FTApplySensitivity()
{
    char szCommand[128];

    sprintf(
        szCommand,
        "scale \"##mouse\" \"##x-axis\" %.6f",
        s_fSensitivity);
    g_pLTClient->RunConsoleString(szCommand);

    sprintf(
        szCommand,
        "scale \"##mouse\" \"##y-axis\" %.6f",
        s_fSensitivity);
    g_pLTClient->RunConsoleString(szCommand);
}

static void FTSaveSettings()
{
    FILE *pFile = fopen("fireteam-settings.cfg", "wt");
    if(!pFile)
    {
        return;
    }

    fprintf(
        pFile,
        "%.6f %u %u %u\n",
        s_fSensitivity,
        s_aResolutions[s_nResolution].nWidth,
        s_aResolutions[s_nResolution].nHeight,
        s_bWindowed ? 1 : 0);

    fclose(pFile);
}

static void FTLoadSettings()
{
    FILE *pFile = fopen("fireteam-settings.cfg", "rt");
    if(!pFile)
    {
        return;
    }

    float fSensitivity = s_fSensitivity;
    uint32 nWidth = 0;
    uint32 nHeight = 0;
    uint32 nWindowed = 1;

    if(fscanf(
        pFile,
        "%f %u %u %u",
        &fSensitivity,
        &nWidth,
        &nHeight,
        &nWindowed) == 4)
    {
        s_fSensitivity = FTClamp(fSensitivity, 0.001000f, 0.020000f);

        for(uint32 i = 0; i < s_nResolutionCount; ++i)
        {
            if(s_aResolutions[i].nWidth == nWidth &&
               s_aResolutions[i].nHeight == nHeight)
            {
                s_nResolution = i;
                break;
            }
        }

        s_bWindowed = (nWindowed != 0);
    }

    fclose(pFile);
}

static void FTApplyVideo()
{
    char szCommand[128];

    sprintf(szCommand, "Windowed %u", s_bWindowed ? 1 : 0);
    g_pLTClient->RunConsoleString(szCommand);

    sprintf(
        szCommand,
        "ResizeScreen %u %u",
        s_aResolutions[s_nResolution].nWidth,
        s_aResolutions[s_nResolution].nHeight);
    g_pLTClient->RunConsoleString(szCommand);

    FTSaveSettings();
}

static void FTSetupQuad(
    LT_POLYF4 &poly,
    float x,
    float y,
    float width,
    float height,
    uint8 r,
    uint8 g,
    uint8 b,
    uint8 a)
{
    poly.rgba.r = r;
    poly.rgba.g = g;
    poly.rgba.b = b;
    poly.rgba.a = a;

    poly.verts[0].x = x;
    poly.verts[0].y = y;
    poly.verts[0].z = SCREEN_NEAR_Z;

    poly.verts[1].x = x + width;
    poly.verts[1].y = y;
    poly.verts[1].z = SCREEN_NEAR_Z;

    poly.verts[2].x = x + width;
    poly.verts[2].y = y + height;
    poly.verts[2].z = SCREEN_NEAR_Z;

    poly.verts[3].x = x;
    poly.verts[3].y = y + height;
    poly.verts[3].z = SCREEN_NEAR_Z;
}

void FT_SettingsInit()
{
    FTReadCurrentVideo();
    FTLoadSettings();
    FTApplySensitivity();

    if(!s_pFont)
    {
        const char *pFontFilename = "fonts/SQR721B.TTF";
        const char *pFontFace = "Square721 BT";

        s_pFont = g_pLTCFontManager->CreateFont(
            pFontFilename,
            pFontFace,
            18,
            33,
            255);

        if(!s_pFont)
        {
            g_pLTClient->CPrint(
                "Fireteam: failed to create settings font.");
            return;
        }

        s_pFont->SetDefCharWidth(5);
        s_pFont->SetDefColor(0xFFFFFFFF);
    }

    s_pTitle = g_pLTCFontManager->CreateFormattedPolyString(
        s_pFont,
        "FIRETEAM");
    s_pBody = g_pLTCFontManager->CreateFormattedPolyString(
        s_pFont,
        "");

    if(s_pTitle)
    {
        s_pTitle->SetColor(0xFFFFB000);
    }

    if(s_pBody)
    {
        s_pBody->SetColor(0xFFFFFFFF);
    }
}

void FT_SettingsTerm()
{
    if(s_pTitle)
    {
        g_pLTCFontManager->DestroyPolyString(s_pTitle);
        s_pTitle = LTNULL;
    }

    if(s_pBody)
    {
        g_pLTCFontManager->DestroyPolyString(s_pBody);
        s_pBody = LTNULL;
    }

    if(s_pFont)
    {
        g_pLTCFontManager->DestroyFont(s_pFont);
        s_pFont = LTNULL;
    }

    s_bOpen = false;
}

void FT_SettingsToggle()
{
    s_bOpen = !s_bOpen;

    if(s_bOpen)
    {
        FTReadCurrentVideo();
        s_nSelected = 0;
        g_pLTClient->ClearInput();
    }
}

bool FT_SettingsIsOpen()
{
    return s_bOpen;
}

bool FT_SettingsHandleKey(int nKey)
{
    if(!s_bOpen)
    {
        return false;
    }

    if(nKey == VK_ESCAPE)
    {
        s_bOpen = false;
        g_pLTClient->ClearInput();
        return true;
    }

    if(nKey == VK_UP)
    {
        s_nSelected = (s_nSelected == 0) ? 5 : (s_nSelected - 1);
        return true;
    }

    if(nKey == VK_DOWN)
    {
        s_nSelected = (s_nSelected + 1) % 6;
        return true;
    }

    if(nKey == VK_LEFT || nKey == VK_RIGHT)
    {
        int nDirection = (nKey == VK_RIGHT) ? 1 : -1;

        if(s_nSelected == 0)
        {
            s_fSensitivity = FTClamp(
                s_fSensitivity + (0.000500f * (float)nDirection),
                0.001000f,
                0.020000f);

            FTApplySensitivity();
            FTSaveSettings();
        }
        else if(s_nSelected == 1)
        {
            int nNew = (int)s_nResolution + nDirection;

            if(nNew < 0)
                nNew = (int)s_nResolutionCount - 1;
            else if(nNew >= (int)s_nResolutionCount)
                nNew = 0;

            s_nResolution = (uint32)nNew;
        }
        else if(s_nSelected == 2)
        {
            s_bWindowed = !s_bWindowed;
        }

        return true;
    }

    if(nKey == VK_RETURN)
    {
        if(s_nSelected == 3)
        {
            FTApplyVideo();
        }
        else if(s_nSelected == 4)
        {
            s_bOpen = false;
            g_pLTClient->ClearInput();
        }
        else if(s_nSelected == 5)
        {
            g_pLTClient->Shutdown();
        }

        return true;
    }

    return true;
}

void FT_SettingsRender()
{
    if(!s_bOpen || !s_pTitle || !s_pBody)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    const float fWidth = 560.0f;
    const float fHeight = 330.0f;
    const float fX = ((float)nScreenW - fWidth) * 0.5f;
    const float fY = ((float)nScreenH - fHeight) * 0.5f;

    LT_POLYF4 background;
    FTSetupQuad(background, fX, fY, fWidth, fHeight, 8, 8, 8, 220);

    g_pLTCDrawPrim->SetTexture(LTNULL);
    g_pLTCDrawPrim->SetTransformType(DRAWPRIM_TRANSFORM_SCREEN);
    g_pLTCDrawPrim->SetColorOp(DRAWPRIM_NOCOLOROP);
    g_pLTCDrawPrim->SetAlphaBlendMode(DRAWPRIM_BLEND_MOD_SRCALPHA);
    g_pLTCDrawPrim->SetZBufferMode(DRAWPRIM_NOZ);
    g_pLTCDrawPrim->SetAlphaTestMode(DRAWPRIM_NOALPHATEST);
    g_pLTCDrawPrim->SetClipMode(DRAWPRIM_FASTCLIP);
    g_pLTCDrawPrim->SetFillMode(DRAWPRIM_FILL);
    g_pLTCDrawPrim->SetCullMode(DRAWPRIM_CULL_NONE);
    g_pLTCDrawPrim->SetCamera(LTNULL);

    g_pLTCDrawPrim->BeginDrawPrim();
    g_pLTCDrawPrim->DrawPrim(&background, 1);
    g_pLTCDrawPrim->EndDrawPrim();

    char szBody[1024];
    float fMultiplier = s_fSensitivity / 0.004625f;

    sprintf(
        szBody,
        "%s Mouse Sensitivity    %.2fx\n"
        "%s Resolution           %u x %u\n"
        "%s Display              %s\n"
        "%s Apply Video\n"
        "%s Resume\n"
        "%s Quit\n\n"
        "Arrow keys: navigate/change     Enter: select     Esc: resume",
        s_nSelected == 0 ? ">" : " ",
        fMultiplier,
        s_nSelected == 1 ? ">" : " ",
        s_aResolutions[s_nResolution].nWidth,
        s_aResolutions[s_nResolution].nHeight,
        s_nSelected == 2 ? ">" : " ",
        s_bWindowed ? "Windowed" : "Fullscreen",
        s_nSelected == 3 ? ">" : " ",
        s_nSelected == 4 ? ">" : " ",
        s_nSelected == 5 ? ">" : " ");

    s_pTitle->SetText("FIRETEAM  -  SETTINGS");
    s_pTitle->SetPosition(fX + 32.0f, fY + 28.0f);

    s_pBody->SetText(szBody);
    s_pBody->SetPosition(fX + 42.0f, fY + 78.0f);

    s_pTitle->Render();
    s_pBody->Render();
}