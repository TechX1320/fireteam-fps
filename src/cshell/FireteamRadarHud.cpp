#include "FireteamRadarHud.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltmessage.h>
#include <iltdrawprim.h>
#include <math.h>

// First-pass Cabin Fever-inspired radar. Drawn using existing Jupiter
// primitives: no asset ZIPs, new shaders, NOLF2 effect groups or CA DTX
// dependencies. Server owns what players are permitted to see.
enum { kRadarCapacity = 64 };
struct FTRadarDot
{
    uint8 nType; // 1 ally, 2 infected, 3 Tanker, 4 Assassin
    int8 nX, nY;
};
static FTRadarDot s_aRadarDots[kRadarCapacity];
static uint8 s_nRadarDots = 0;
static float s_fLastRadarPacket = -10.0f;

void FT_RadarHudReset()
{
    s_nRadarDots = 0;
    s_fLastRadarPacket = -10.0f;
}

void FT_RadarHudHandleMessage(ILTMessage_Read *pMessage)
{
    if(!pMessage || !g_pLTClient) return;
    const uint8 count = pMessage->Readuint8();
    if(count > kRadarCapacity)
    {
        s_nRadarDots = 0;
        return;
    }
    for(uint8 n = 0; n < count; ++n)
    {
        s_aRadarDots[n].nType = pMessage->Readuint8();
        s_aRadarDots[n].nX = pMessage->Readint8();
        s_aRadarDots[n].nY = pMessage->Readint8();
    }
    s_nRadarDots = count;
    s_fLastRadarPacket = g_pLTClient->GetTime();
}

// Small circular radar, anchored in the same corner as Combat Arms.
// Screen-space polies are cheap even with dozens of overlapping infected.
static LT_POLYF4 FT_RadarRect(
    float x, float y, float width, float height,
    uint8 red, uint8 green, uint8 blue, uint8 alpha)
{
    LT_POLYF4 p;
    p.rgba.r = red;
    p.rgba.g = green;
    p.rgba.b = blue;
    p.rgba.a = alpha;
    p.verts[0].x = x;
    p.verts[0].y = y;
    p.verts[1].x = x + width;
    p.verts[1].y = y;
    p.verts[2].x = x + width;
    p.verts[2].y = y + height;
    p.verts[3].x = x;
    p.verts[3].y = y + height;
    for(uint32 i = 0; i < 4; ++i)
        p.verts[i].z = SCREEN_NEAR_Z;
    return p;
}

static LT_POLYF4 FT_RadarRing(
    float cx, float cy, float inner, float outer,
    float a, float b, uint8 alpha)
{
    LT_POLYF4 p;
    p.rgba.r = 67;
    p.rgba.g = 178;
    p.rgba.b = 195;
    p.rgba.a = alpha;
    const float x[4] = {
        cx + inner * cosf(a), cx + inner * cosf(b),
        cx + outer * cosf(b), cx + outer * cosf(a)
    };
    const float y[4] = {
        cy + inner * sinf(a), cy + inner * sinf(b),
        cy + outer * sinf(b), cy + outer * sinf(a)
    };
    for(uint32 i = 0; i < 4; ++i)
    {
        p.verts[i].x = x[i];
        p.verts[i].y = y[i];
        p.verts[i].z = SCREEN_NEAR_Z;
    }
    return p;
}

void FT_RenderRadarHud()
{
    if(!g_pLTClient || !g_pLTCDrawPrim) return;

    // Optional toggle, e.g. +ftradarenabled 0. Defaults ON if not defined.
    HCONSOLEVAR toggle = g_pLTClient->GetConsoleVar("ftradarenabled");
    if(toggle && g_pLTClient->GetVarValueFloat(toggle) < 0.5f)
        return;

    uint32 screenW = 0, screenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(), &screenW, &screenH);
    if(screenW < 480 || screenH < 320) return;

    float radius = (float)screenH * 0.090f;
    if(radius < 54.0f) radius = 54.0f;
    if(radius > 84.0f) radius = 84.0f;
    const float cx = (float)screenW - radius - 24.0f;
    const float cy = radius + 30.0f;

    g_pLTCDrawPrim->SetCamera(LTNULL);
    g_pLTCDrawPrim->SetTexture(LTNULL);
    g_pLTCDrawPrim->SetTransformType(DRAWPRIM_TRANSFORM_SCREEN);
    g_pLTCDrawPrim->SetColorOp(DRAWPRIM_NOCOLOROP);
    g_pLTCDrawPrim->SetAlphaBlendMode(DRAWPRIM_BLEND_MOD_SRCALPHA);
    g_pLTCDrawPrim->SetZBufferMode(DRAWPRIM_NOZ);
    g_pLTCDrawPrim->SetAlphaTestMode(DRAWPRIM_NOALPHATEST);
    g_pLTCDrawPrim->SetClipMode(DRAWPRIM_FASTCLIP);
    g_pLTCDrawPrim->SetFillMode(DRAWPRIM_FILL);
    g_pLTCDrawPrim->SetCullMode(DRAWPRIM_CULL_NONE);
    g_pLTCDrawPrim->BeginDrawPrim();

    // The background is a stack of thin horizontal strips clipped to
    // a circle, rather than a square obscuring the screen corner.
    const uint32 slices = 26;
    for(uint32 n = 0; n < slices; ++n)
    {
        const float y = -radius + (n + 0.5f) *
            (radius * 2.0f) / slices;
        const float halfWidth = sqrtf(radius * radius - y * y);
        LT_POLYF4 strip = FT_RadarRect(
            cx - halfWidth, cy + y - radius / slices,
            2.0f * halfWidth, (radius * 2.0f) / slices + 0.6f,
            12, 65, 82, 95);
        g_pLTCDrawPrim->DrawPrim(&strip, 1);
    }

    for(uint32 n = 0; n < 40; ++n)
    {
        const float a = (float)n * 6.2831853f / 40.0f;
        const float b = (float)(n + 1) * 6.2831853f / 40.0f;
        LT_POLYF4 rim = FT_RadarRing(
            cx, cy, radius - 1.0f, radius + 1.5f, a, b, 195);
        g_pLTCDrawPrim->DrawPrim(&rim, 1);
    }

    LT_POLYF4 horizontal = FT_RadarRect(
        cx - radius + 5, cy, 2 * radius - 10, 1,
        85, 164, 174, 55);
    LT_POLYF4 vertical = FT_RadarRect(
        cx, cy - radius + 5, 1, 2 * radius - 10,
        85, 164, 174, 55);
    g_pLTCDrawPrim->DrawPrim(&horizontal, 1);
    g_pLTCDrawPrim->DrawPrim(&vertical, 1);

    // Updates are disposable, not accumulated network events.
    const float age = g_pLTClient->GetTime() - s_fLastRadarPacket;
    if(age >= 0.0f && age < 2.5f)
    {
        for(uint8 n = 0; n < s_nRadarDots; ++n)
        {
            const FTRadarDot &dot = s_aRadarDots[n];
            const float dx = (float)dot.nX * (radius - 6.0f) / 115.0f;
            const float dy = (float)dot.nY * (radius - 6.0f) / 115.0f;
            if(dx * dx + dy * dy >
               (radius - 5.0f) * (radius - 5.0f))
                continue;

            uint8 red = 240, green = 70, blue = 65;
            float size = 3.6f;
            if(dot.nType == 1)
            {
                red = 72; green = 236; blue = 180; size = 5.0f;
            }
            else if(dot.nType == 3)
            {
                red = 255; green = 174; blue = 35; size = 6.0f;
            }
            else if(dot.nType == 4)
            {
                red = 255; green = 80; blue = 170; size = 4.5f;
            }
            else if(dot.nType != 2) continue;

            LT_POLYF4 marker = FT_RadarRect(
                cx + dx - size * 0.5f,
                cy - dy - size * 0.5f,
                size, size, red, green, blue, 238);
            g_pLTCDrawPrim->DrawPrim(&marker, 1);
        }
    }

    // Bright central self marker, heading always points up.
    LT_POLYF4 self = FT_RadarRect(
        cx - 2.0f, cy - 4.5f, 4.0f, 8.0f,
        240, 248, 251, 255);
    LT_POLYF4 tip = FT_RadarRect(
        cx - 1.0f, cy - 6.0f, 2.0f, 2.0f,
        240, 248, 251, 255);
    g_pLTCDrawPrim->DrawPrim(&self, 1);
    g_pLTCDrawPrim->DrawPrim(&tip, 1);
    g_pLTCDrawPrim->EndDrawPrim();
}
