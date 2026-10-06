#include "FireteamSettingsMenu.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltdrawprim.h>
#include <iltfontmanager.h>
#include <iltsoundmgr.h>
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
static float s_fSensitivityX = 0.001500f;
static float s_fSensitivityY = 0.001500f;
static uint32 s_nSoundVolume = 100;
static float s_fGamma = 1.0f;

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
        s_fSensitivityX);
    g_pLTClient->RunConsoleString(szCommand);

    sprintf(
        szCommand,
        "scale \"##mouse\" \"##y-axis\" %.6f",
        s_fSensitivityY);
    g_pLTClient->RunConsoleString(szCommand);
}

static void FTApplySoundVolume()
{
    if(g_pLTCSoundMgr)
    {
        g_pLTCSoundMgr->SetVolume(
            (short)s_nSoundVolume);
    }
}

static void FTApplyGamma()
{
    char szCommand[64];

    sprintf(szCommand, "GammaR %.3f", s_fGamma);
    g_pLTClient->RunConsoleString(szCommand);
    sprintf(szCommand, "GammaG %.3f", s_fGamma);
    g_pLTClient->RunConsoleString(szCommand);
    sprintf(szCommand, "GammaB %.3f", s_fGamma);
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
        "%.6f %.6f %u %.3f %u %u %u\n",
        s_fSensitivityX,
        s_fSensitivityY,
        s_nSoundVolume,
        s_fGamma,
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

    char szLine[256];
    if(!fgets(szLine, sizeof(szLine), pFile))
    {
        fclose(pFile);
        return;
    }

    float fSensitivityX = s_fSensitivityX;
    float fSensitivityY = s_fSensitivityY;
    uint32 nSoundVolume = s_nSoundVolume;
    float fGamma = s_fGamma;
    uint32 nWidth = 0;
    uint32 nHeight = 0;
    uint32 nWindowed = 1;

    int nRead = sscanf(
        szLine,
        "%f %f %u %f %u %u %u",
        &fSensitivityX,
        &fSensitivityY,
        &nSoundVolume,
        &fGamma,
        &nWidth,
        &nHeight,
        &nWindowed);

    if(nRead != 7)
    {
        // 2026-10 format before gamma was added.
        if(sscanf(
            szLine,
            "%f %f %u %u %u %u",
            &fSensitivityX,
            &fSensitivityY,
            &nSoundVolume,
            &nWidth,
            &nHeight,
            &nWindowed) == 6)
        {
            fGamma = 1.0f;
            nRead = 7;
        }
    }

    if(nRead != 7)
    {
        // Previous format: X sensitivity, Y sensitivity, width, height, windowed.
        if(sscanf(
            szLine,
            "%f %f %u %u %u",
            &fSensitivityX,
            &fSensitivityY,
            &nWidth,
            &nHeight,
            &nWindowed) == 5)
        {
            nSoundVolume = 100;
            fGamma = 1.0f;
            nRead = 7;
        }
    }

    if(nRead != 7)
    {
        // Original format: one sensitivity, width, height, windowed.
        float fLegacySensitivity = s_fSensitivityX;
        if(sscanf(
            szLine,
            "%f %u %u %u",
            &fLegacySensitivity,
            &nWidth,
            &nHeight,
            &nWindowed) == 4)
        {
            fSensitivityX = fLegacySensitivity;
            fSensitivityY = fLegacySensitivity;
            nSoundVolume = 100;
            fGamma = 1.0f;
            nRead = 7;
        }
    }

    if(nRead == 7)
    {
        // Much finer low-end mouse range. The old 0.0005 floor/step was too
        // coarse for modern high-DPI mice.
        s_fSensitivityX =
            FTClamp(fSensitivityX, 0.000050f, 0.020000f);
        s_fSensitivityY =
            FTClamp(fSensitivityY, 0.000050f, 0.020000f);

        if(nSoundVolume > 100)
        {
            nSoundVolume = 100;
        }
        s_nSoundVolume = nSoundVolume;

        // Matches the range used by NOLF2's original display screen.
        s_fGamma = FTClamp(fGamma, 0.50f, 6.00f);

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
    FTApplySoundVolume();
    FTApplyGamma();

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
        s_nSelected = (s_nSelected == 0) ? 8 : (s_nSelected - 1);
        return true;
    }

    if(nKey == VK_DOWN)
    {
        s_nSelected = (s_nSelected + 1) % 9;
        return true;
    }

    if(nKey == VK_LEFT || nKey == VK_RIGHT)
    {
        int nDirection = (nKey == VK_RIGHT) ? 1 : -1;

        if(s_nSelected == 0)
        {
            s_fSensitivityX = FTClamp(
                s_fSensitivityX + (0.000100f * (float)nDirection),
                0.000050f,
                0.020000f);

            FTApplySensitivity();
            FTSaveSettings();
        }
        else if(s_nSelected == 1)
        {
            s_fSensitivityY = FTClamp(
                s_fSensitivityY + (0.000100f * (float)nDirection),
                0.000050f,
                0.020000f);

            FTApplySensitivity();
            FTSaveSettings();
        }
        else if(s_nSelected == 2)
        {
            int nVolume =
                (int)s_nSoundVolume +
                (5 * nDirection);

            if(nVolume < 0) nVolume = 0;
            if(nVolume > 100) nVolume = 100;

            s_nSoundVolume = (uint32)nVolume;
            FTApplySoundVolume();
            FTSaveSettings();
        }
        else if(s_nSelected == 3)
        {
            s_fGamma = FTClamp(
                s_fGamma + (0.10f * (float)nDirection),
                0.50f,
                6.00f);

            FTApplyGamma();
            FTSaveSettings();
        }
        else if(s_nSelected == 4)
        {
            int nNew = (int)s_nResolution + nDirection;

            if(nNew < 0)
                nNew = (int)s_nResolutionCount - 1;
            else if(nNew >= (int)s_nResolutionCount)
                nNew = 0;

            s_nResolution = (uint32)nNew;
        }
        else if(s_nSelected == 5)
        {
            s_bWindowed = !s_bWindowed;
        }

        return true;
    }

    if(nKey == VK_RETURN)
    {
        if(s_nSelected == 6)
        {
            FTApplyVideo();
        }
        else if(s_nSelected == 7)
        {
            s_bOpen = false;
            g_pLTClient->ClearInput();
        }
        else if(s_nSelected == 8)
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
    const float fHeight = 415.0f;
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
    float fMultiplierX = s_fSensitivityX / 0.004625f;
    float fMultiplierY = s_fSensitivityY / 0.004625f;

    sprintf(
        szBody,
        "%s Horizontal Sensitivity  %.2fx\n"
        "%s Vertical Sensitivity    %.2fx\n"
        "%s Game Volume             %u%%\n"
        "%s Brightness / Gamma      %.2fx\n"
        "%s Resolution              %u x %u\n"
        "%s Display                 %s\n"
        "%s Apply Video\n"
        "%s Resume\n"
        "%s Quit\n\n"
        "Arrow keys: navigate/change     Enter: select     Esc: resume",
        s_nSelected == 0 ? ">" : " ",
        fMultiplierX,
        s_nSelected == 1 ? ">" : " ",
        fMultiplierY,
        s_nSelected == 2 ? ">" : " ",
        s_nSoundVolume,
        s_nSelected == 3 ? ">" : " ",
        s_fGamma,
        s_nSelected == 4 ? ">" : " ",
        s_aResolutions[s_nResolution].nWidth,
        s_aResolutions[s_nResolution].nHeight,
        s_nSelected == 5 ? ">" : " ",
        s_bWindowed ? "Windowed" : "Fullscreen",
        s_nSelected == 6 ? ">" : " ",
        s_nSelected == 7 ? ">" : " ",
        s_nSelected == 8 ? ">" : " ");

    s_pTitle->SetText("FIRETEAM  -  SETTINGS");
    s_pTitle->SetPosition(fX + 32.0f, fY + 28.0f);

    s_pBody->SetText(szBody);
    s_pBody->SetPosition(fX + 42.0f, fY + 78.0f);

    s_pTitle->Render();
    s_pBody->Render();
}