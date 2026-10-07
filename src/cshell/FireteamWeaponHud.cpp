#include "FireteamWeaponHud.h"
#include "clientinterfaces.h"
#include "FireteamWeaponDefs.h"

#include <iltclient.h>
#include <iltdrawprim.h>
#include <iltfontmanager.h>
#include <stdio.h>

static CUIFont *s_pAmmoFont = LTNULL;
static CUIFont *s_pQaFont = LTNULL;
static CUIFormattedPolyString *s_pAmmoText = LTNULL;
static CUIFormattedPolyString *s_pWeaponName = LTNULL;
static CUIFormattedPolyString *s_pQaMenuText = LTNULL;
static FTWeaponDef s_WeaponDefs[6];

static uint16 s_nPrimaryClip = 30;
static uint16 s_nPrimaryReserve = 90;

static float s_fCrosshairKick = 0.0f;
static float s_fLastCrosshairTime = 0.0f;
static uint8 s_nCrosshairSlot = 0;
static uint32 s_nQaCurrent = 0;
static uint32 s_nQaTotal = 0;
static bool s_bQaQuarantined = false;
static bool s_bQaMenuOpen = false;
static bool s_bQaZombiesEnabled = false;
static float s_fQaMoveStep = 0.10f;

static void FT_SetupQuad(
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

static void FT_SetDrawState()
{
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
}

void FT_WeaponHudInit()
{
    if(s_pAmmoFont)
    {
        return;
    }

    FT_LoadWeaponDefs("config/weapons.cfg", s_WeaponDefs);

    s_pAmmoFont = g_pLTCFontManager->CreateFont(
        "fonts/SQR721B.TTF",
        "Square721 BT",
        22,
        36,
        255);

    if(!s_pAmmoFont)
    {
        g_pLTClient->CPrint("Fireteam: failed to create weapon HUD font.");
        return;
    }

    s_pAmmoFont->SetDefCharWidth(6);
    s_pAmmoFont->SetDefColor(0xFFFFFFFF);

    s_pAmmoText = g_pLTCFontManager->CreateFormattedPolyString(
        s_pAmmoFont,
        "30/90");

    if(s_pAmmoText)
    {
        s_pAmmoText->SetColor(0xFFFFFFFF);
    }

    s_pWeaponName = g_pLTCFontManager->CreateFormattedPolyString(
        s_pAmmoFont,
        "Weapon");

    if(s_pWeaponName)
    {
        s_pWeaponName->SetColor(0xFFFFFFFF);
    }

    s_pQaFont = g_pLTCFontManager->CreateFont(
        "fonts/SQR721B.TTF",
        "Square721 BT",
        14,
        22,
        255);

    if(s_pQaFont)
    {
        s_pQaFont->SetDefCharWidth(5);
        s_pQaFont->SetDefColor(0xFFFFFFFF);

        s_pQaMenuText =
            g_pLTCFontManager->CreateFormattedPolyString(
                s_pQaFont,
                "FIRETEAM QA TOOLS");

        if(s_pQaMenuText)
        {
            s_pQaMenuText->SetColor(0xFFFFFFFF);
        }
    }
}

void FT_WeaponHudTerm()
{
    if(s_pQaMenuText)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pQaMenuText);
        s_pQaMenuText = LTNULL;
    }

    if(s_pQaFont)
    {
        g_pLTCFontManager->DestroyFont(
            s_pQaFont);
        s_pQaFont = LTNULL;
    }

    if(s_pWeaponName)
    {
        g_pLTCFontManager->DestroyPolyString(s_pWeaponName);
        s_pWeaponName = LTNULL;
    }

    if(s_pAmmoText)
    {
        g_pLTCFontManager->DestroyPolyString(s_pAmmoText);
        s_pAmmoText = LTNULL;
    }

    if(s_pAmmoFont)
    {
        g_pLTCFontManager->DestroyFont(s_pAmmoFont);
        s_pAmmoFont = LTNULL;
    }
}

void FT_SetPrimaryAmmo(uint16 nClip, uint16 nReserve)
{
    s_nPrimaryClip = nClip;
    s_nPrimaryReserve = nReserve;
}

void FT_WeaponHudSetWeaponDefinition(
    uint8 nWeaponSlot,
    const FTWeaponDef *pDef)
{
    if(!pDef ||
       nWeaponSlot < 1 ||
       nWeaponSlot > 5)
    {
        return;
    }

    s_WeaponDefs[nWeaponSlot] =
        *pDef;
}

void FT_WeaponHudSetQaProgress(
    uint32 nCurrent,
    uint32 nTotal)
{
    s_nQaCurrent = nCurrent;
    s_nQaTotal = nTotal;
}

void FT_WeaponHudSetQaQuarantined(
    bool bQuarantined)
{
    s_bQaQuarantined =
        bQuarantined;
}

void FT_WeaponHudSetQaMenu(
    bool bOpen,
    float fMoveStep,
    bool bZombiesEnabled)
{
    s_bQaMenuOpen =
        bOpen;
    s_bQaZombiesEnabled =
        bZombiesEnabled;

    if(fMoveStep > 0.0f)
    {
        s_fQaMoveStep =
            fMoveStep;
    }
}

void FT_WeaponHudOnShot(uint8 nWeaponSlot)
{
    const FTWeaponDef *pDef =
        FT_GetWeaponDef(s_WeaponDefs, nWeaponSlot);

    if(!pDef)
    {
        return;
    }

    if(s_nCrosshairSlot != nWeaponSlot)
    {
        s_nCrosshairSlot = nWeaponSlot;
        s_fCrosshairKick = 0.0f;
    }

    s_fCrosshairKick += pDef->fCrosshairShotKick;

    const float fMaxExtra =
        pDef->fCrosshairMaxGap > pDef->fCrosshairBaseGap
        ? (pDef->fCrosshairMaxGap - pDef->fCrosshairBaseGap)
        : 0.0f;

    if(s_fCrosshairKick > fMaxExtra)
    {
        s_fCrosshairKick = fMaxExtra;
    }
}

void FT_RenderWeaponHud(
    uint8 nWeaponSlot,
    bool bFirstPerson,
    bool bShowCrosshair,
    bool bShowAmmo,
    bool bMoving,
    bool bScoped)
{
    if(!bFirstPerson ||
       nWeaponSlot < 1 ||
       nWeaponSlot > 5 ||
       !g_pLTClient || !g_pLTCDrawPrim)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    const FTWeaponDef *pDef =
        FT_GetWeaponDef(s_WeaponDefs, nWeaponSlot);

    if(bScoped)
    {
        const float fScreenW = (float)nScreenW;
        const float fScreenH = (float)nScreenH;
        const float fScopeSize =
            fScreenW < fScreenH ? fScreenW : fScreenH;
        const float fScopeLeft =
            (fScreenW - fScopeSize) * 0.5f;
        const float fScopeTop =
            (fScreenH - fScopeSize) * 0.5f;
        const float cx = fScreenW * 0.5f;
        const float cy = fScreenH * 0.5f;

        LT_POLYF4 mask[8];

        // Black out everything outside a centered scope viewport.
        FT_SetupQuad(mask[0], 0.0f, 0.0f, fScopeLeft, fScreenH, 0, 0, 0, 255);
        FT_SetupQuad(mask[1], fScopeLeft + fScopeSize, 0.0f,
            fScreenW - (fScopeLeft + fScopeSize), fScreenH, 0, 0, 0, 255);
        FT_SetupQuad(mask[2], fScopeLeft, 0.0f, fScopeSize, fScopeTop, 0, 0, 0, 255);
        FT_SetupQuad(mask[3], fScopeLeft, fScopeTop + fScopeSize, fScopeSize,
            fScreenH - (fScopeTop + fScopeSize), 0, 0, 0, 255);

        // Simple optic reticle. This can later be replaced by an authored CA
        // scope texture without changing zoom/gameplay behavior.
        FT_SetupQuad(mask[4], fScopeLeft, cy - 0.5f, fScopeSize, 1.0f,
            0, 0, 0, 210);
        FT_SetupQuad(mask[5], cx - 0.5f, fScopeTop, 1.0f, fScopeSize,
            0, 0, 0, 210);
        FT_SetupQuad(mask[6], cx - 3.0f, cy - 3.0f, 6.0f, 6.0f,
            0, 0, 0, 230);
        FT_SetupQuad(mask[7], fScopeLeft, fScopeTop, fScopeSize, 2.0f,
            0, 0, 0, 180);

        FT_SetDrawState();
        g_pLTCDrawPrim->BeginDrawPrim();
        g_pLTCDrawPrim->DrawPrim(mask, 8);
        g_pLTCDrawPrim->EndDrawPrim();
    }

    const float fNow = g_pLTClient->GetTime();
    if(s_fLastCrosshairTime <= 0.0f)
    {
        s_fLastCrosshairTime = fNow;
    }

    float fDelta = fNow - s_fLastCrosshairTime;
    if(fDelta < 0.0f) fDelta = 0.0f;
    if(fDelta > 0.25f) fDelta = 0.25f;
    s_fLastCrosshairTime = fNow;

    if(s_nCrosshairSlot != nWeaponSlot)
    {
        s_nCrosshairSlot = nWeaponSlot;
        s_fCrosshairKick = 0.0f;
    }

    if(pDef && s_fCrosshairKick > 0.0f)
    {
        s_fCrosshairKick -= pDef->fCrosshairRecover * fDelta;
        if(s_fCrosshairKick < 0.0f)
        {
            s_fCrosshairKick = 0.0f;
        }
    }

    if(bShowCrosshair && pDef)
    {
        const float cx = (float)nScreenW * 0.5f;
        const float cy = (float)nScreenH * 0.5f;

        // NOLF2 HUDCrosshair uses screen-centered DrawPrim geometry. Keep the
        // same basic approach here without depending on NOLF2 texture assets.
        float fGap =
            pDef->fCrosshairBaseGap +
            s_fCrosshairKick +
            (bMoving ? pDef->fCrosshairMoveKick : 0.0f);

        if(pDef->fCrosshairMaxGap > 0.0f &&
           fGap > pDef->fCrosshairMaxGap)
        {
            fGap = pDef->fCrosshairMaxGap;
        }

        const float fLength = 10.0f;
        const float fThickness = 2.0f;

        LT_POLYF4 crosshair[4];

        FT_SetupQuad(
            crosshair[0],
            cx - fGap - fLength,
            cy - (fThickness * 0.5f),
            fLength,
            fThickness,
            0, 255, 255, 235);

        FT_SetupQuad(
            crosshair[1],
            cx + fGap,
            cy - (fThickness * 0.5f),
            fLength,
            fThickness,
            0, 255, 255, 235);

        FT_SetupQuad(
            crosshair[2],
            cx - (fThickness * 0.5f),
            cy - fGap - fLength,
            fThickness,
            fLength,
            0, 255, 255, 235);

        FT_SetupQuad(
            crosshair[3],
            cx - (fThickness * 0.5f),
            cy + fGap,
            fThickness,
            fLength,
            0, 255, 255, 235);

        FT_SetDrawState();

        g_pLTCDrawPrim->BeginDrawPrim();
        g_pLTCDrawPrim->DrawPrim(crosshair, 4);
        g_pLTCDrawPrim->EndDrawPrim();

    }

    if(s_pWeaponName &&
       pDef &&
       !(s_nQaTotal > 0 &&
         s_bQaMenuOpen))
    {
        char szWeaponLabel[192];

        if(s_nQaTotal > 0)
        {
            sprintf(
                szWeaponLabel,
                "QA %u/%u  %s%s  [X %.2f Y %.2f Z %.2f S %.2f]  [INSERT/F10: TOOLS]",
                s_nQaCurrent,
                s_nQaTotal,
                s_bQaQuarantined
                    ? "[DISABLED] "
                    : "",
                pDef->sName,
                pDef->fViewX,
                pDef->fViewY,
                pDef->fViewZ,
                pDef->fViewScale);
        }
        else
        {
            strncpy(
                szWeaponLabel,
                pDef->sName,
                sizeof(szWeaponLabel) - 1);
            szWeaponLabel[
                sizeof(szWeaponLabel) - 1] =
                '\0';
        }

        s_pWeaponName->SetText(szWeaponLabel);
        if(s_nQaTotal > 0)
        {
            // QA names can be much longer than production weapon labels.
            // Anchor them from the left so variant names never disappear off
            // the right edge while rapidly auditing the catalog.
            s_pWeaponName->SetPosition(
                18.0f,
                28.0f);
        }
        else
        {
            s_pWeaponName->SetPosition(
                (float)nScreenW - 210.0f,
                (float)nScreenH - 102.0f);
        }
        s_pWeaponName->Render();
    }

    if(s_nQaTotal > 0 &&
       s_bQaMenuOpen &&
       pDef)
    {
        const float fPanelX = 18.0f;
        const float fPanelY = 58.0f;
        const float fPanelW = 540.0f;
        const float fPanelH = 346.0f;

        LT_POLYF4 qaPanel[4];

        FT_SetupQuad(
            qaPanel[0],
            fPanelX,
            fPanelY,
            fPanelW,
            fPanelH,
            7,
            9,
            11,
            232);

        FT_SetupQuad(
            qaPanel[1],
            fPanelX,
            fPanelY,
            4.0f,
            fPanelH,
            0,
            220,
            255,
            255);

        FT_SetupQuad(
            qaPanel[2],
            fPanelX,
            fPanelY,
            fPanelW,
            2.0f,
            255,
            184,
            0,
            255);

        FT_SetupQuad(
            qaPanel[3],
            fPanelX + 14.0f,
            fPanelY + 70.0f,
            fPanelW - 28.0f,
            1.0f,
            72,
            78,
            84,
            220);

        FT_SetDrawState();
        g_pLTCDrawPrim->BeginDrawPrim();
        g_pLTCDrawPrim->DrawPrim(
            qaPanel,
            4);
        g_pLTCDrawPrim->EndDrawPrim();

        if(s_pQaMenuText)
        {
            char szQaMenu[1200];

            sprintf(
                szQaMenu,
                "FIRETEAM QA TOOLS  /  WEAPON POSITION\n"
                "%s%s   QA %u/%u\n"
                "X %.2f   Y %.2f   Z %.2f   SCALE %.2f   STEP %.2f\n"
                "ZOMBIES: %s\n"
                "\n"
                "LEFT / RIGHT       move X\n"
                "UP / DOWN          move Y\n"
                "CTRL + UP / DOWN   move Z\n"
                "[ / ]              shrink / grow model (0.05)\n"
                "+ / -              change move increment\n"
                "AUTO-SAVE          on weapon change / panel close\n"
                "CTRL + S           save position + scale now\n"
                "Q                  enable / disable weapon\n"
                "Z                  toggle zombies\n"
                "MOUSE WHEEL        previous / next weapon\n"
                "INSERT / F10       close QA tools",
                s_bQaQuarantined
                    ? "[DISABLED] "
                    : "",
                pDef->sName,
                s_nQaCurrent,
                s_nQaTotal,
                pDef->fViewX,
                pDef->fViewY,
                pDef->fViewZ,
                pDef->fViewScale,
                s_fQaMoveStep,
                s_bQaZombiesEnabled
                    ? "YES"
                    : "NO");

            s_pQaMenuText->SetText(
                szQaMenu);
            s_pQaMenuText->SetPosition(
                fPanelX + 18.0f,
                fPanelY + 16.0f);
            s_pQaMenuText->Render();
        }
    }

    if(!bShowAmmo)
    {
        return;
    }

    if(!s_pAmmoText)
    {
        FT_WeaponHudInit();
    }

    if(s_pAmmoText)
    {
        char szAmmo[32];

        // NOLF2 HUDAmmo convention: clip / reserve.
        sprintf(
            szAmmo,
            "%u/%u",
            (uint32)s_nPrimaryClip,
            (uint32)s_nPrimaryReserve);

        s_pAmmoText->SetText(szAmmo);
        s_pAmmoText->SetPosition(
            (float)nScreenW - 118.0f,
            (float)nScreenH - 72.0f);
        s_pAmmoText->Render();
    }
}