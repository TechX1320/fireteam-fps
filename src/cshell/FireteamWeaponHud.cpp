#include "FireteamWeaponHud.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltdrawprim.h>
#include <iltfontmanager.h>
#include <stdio.h>

static CUIFont *s_pAmmoFont = LTNULL;
static CUIFormattedPolyString *s_pAmmoText = LTNULL;

static uint16 s_nPrimaryClip = 30;
static uint16 s_nPrimaryReserve = 90;

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
}

void FT_WeaponHudTerm()
{
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

bool FT_ConsumePrimaryAmmoLocal()
{
    if(s_nPrimaryClip == 0)
    {
        return false;
    }

    --s_nPrimaryClip;
    return true;
}

void FT_RenderWeaponHud(uint8 nWeaponSlot, bool bFirstPerson)
{
    if(!bFirstPerson || nWeaponSlot != 1 ||
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

    const float cx = (float)nScreenW * 0.5f;
    const float cy = (float)nScreenH * 0.5f;

    // Ported from NOLF2's DrawPrim crosshair construction: four posts
    // surrounding a center gap.
    const float fGap = 6.0f;
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