#include "FireteamHealthHud.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltdrawprim.h>

static uint8 s_nHealth = 100;
static uint8 s_nMaxHealth = 100;

static void SetupQuad(LT_POLYF4 &poly, float x, float y, float width, float height,
                      uint8 r, uint8 g, uint8 b, uint8 a)
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

void FT_SetHealth(uint8 nHealth, uint8 nMaxHealth)
{
    s_nMaxHealth = nMaxHealth ? nMaxHealth : 1;
    s_nHealth = (nHealth > s_nMaxHealth) ? s_nMaxHealth : nHealth;
}

void FT_RenderPoisonOverlay(HLOCALOBJ hPlayer)
{
    // Temporarily disabled. Our first PoisonGas approximation reports the
    // Cabin Fever spawn interior as inside gas, so the whole screen was tinted.
    // Keep the hook for the accurate CA volume/ClientFX implementation later.
    (void)hPlayer;
}

void FT_RenderHealthHud()
{
    if(!g_pLTClient || !g_pLTCDrawPrim)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nScreenW, &nScreenH);

    const float fWidth = 240.0f;
    const float fHeight = 16.0f;
    const float fX = 28.0f;
    const float fY = (float)nScreenH - 42.0f;
    const float fRatio = (float)s_nHealth / (float)s_nMaxHealth;

    LT_POLYF4 background;
    LT_POLYF4 health;

    SetupQuad(background, fX, fY, fWidth, fHeight, 20, 20, 20, 220);
    SetupQuad(health, fX + 2.0f, fY + 2.0f, (fWidth - 4.0f) * fRatio, fHeight - 4.0f,
              210, 36, 36, 255);

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
    if(s_nHealth > 0)
    {
        g_pLTCDrawPrim->DrawPrim(&health, 1);
    }
    g_pLTCDrawPrim->EndDrawPrim();
}