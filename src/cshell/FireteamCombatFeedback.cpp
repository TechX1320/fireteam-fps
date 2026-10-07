#include "FireteamCombatFeedback.h"
#include "clientinterfaces.h"
#include "msgids.h"

#include <iltclient.h>
#include <iltdrawprim.h>
#include <iltfontmanager.h>
#include <iltmessage.h>
#include <iltsoundmgr.h>
#include <ilttexinterface.h>

#include <string.h>

struct FTFeedbackImage
{
    HTEXTURE hTexture;
    uint32 nWidth;
    uint32 nHeight;
};

enum
{
    FT_FEEDBACK_ROUNDSTART = 100,
    FT_FEEDBACK_QUEUE_SIZE = 16
};

static FTFeedbackImage s_Headshot = { LTNULL, 0, 0 };
static FTFeedbackImage s_Nutshot = { LTNULL, 0, 0 };
static FTFeedbackImage s_FirstKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_DoubleKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_MultiKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_UltraKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_Fantastic = { LTNULL, 0, 0 };
static FTFeedbackImage s_Unbelievable = { LTNULL, 0, 0 };
static FTFeedbackImage s_RoundStart = { LTNULL, 0, 0 };

static CUIFont *s_pFeedbackFont = LTNULL;
static CUIFormattedPolyString *s_pFallback = LTNULL;

static uint8 s_aQueue[FT_FEEDBACK_QUEUE_SIZE];
static uint8 s_nQueueHead = 0;
static uint8 s_nQueueCount = 0;
static uint8 s_nActiveFeedback = 0;
static float s_fActiveStart = 0.0f;

static void FT_LoadFeedbackImage(
    FTFeedbackImage &image,
    const char *pPath)
{
    image.hTexture = LTNULL;
    image.nWidth = 0;
    image.nHeight = 0;

    if(!g_pLTCTexInterface ||
       !pPath ||
       !pPath[0])
    {
        return;
    }

    if(g_pLTCTexInterface->CreateTextureFromName(
           image.hTexture,
           pPath) == LT_OK &&
       image.hTexture)
    {
        g_pLTCTexInterface->GetTextureDims(
            image.hTexture,
            image.nWidth,
            image.nHeight);
    }
}

static void FT_ReleaseFeedbackImage(
    FTFeedbackImage &image)
{
    if(image.hTexture &&
       g_pLTCTexInterface)
    {
        g_pLTCTexInterface->ReleaseTextureHandle(
            image.hTexture);
    }

    image.hTexture = LTNULL;
    image.nWidth = 0;
    image.nHeight = 0;
}

static FTFeedbackImage* FT_ImageForFeedback(
    uint8 nFeedback)
{
    switch(nFeedback)
    {
        case FT_COMBAT_FEEDBACK_HEADSHOT:
            return &s_Headshot;
        case FT_COMBAT_FEEDBACK_NUTSHOT:
            return &s_Nutshot;
        case FT_COMBAT_FEEDBACK_FIRSTKILL:
            return &s_FirstKill;
        case FT_COMBAT_FEEDBACK_DOUBLEKILL:
            return &s_DoubleKill;
        case FT_COMBAT_FEEDBACK_MULTIKILL:
            return &s_MultiKill;
        case FT_COMBAT_FEEDBACK_ULTRAKILL:
            return &s_UltraKill;
        case FT_COMBAT_FEEDBACK_FANTASTIC:
            return &s_Fantastic;
        case FT_COMBAT_FEEDBACK_UNBELIEVABLE:
            return &s_Unbelievable;
        case FT_FEEDBACK_ROUNDSTART:
            return &s_RoundStart;
        default:
            return LTNULL;
    }
}

static const char* FT_TextForFeedback(
    uint8 nFeedback)
{
    switch(nFeedback)
    {
        case FT_COMBAT_FEEDBACK_HEADSHOT:
            return "HEADSHOT";
        case FT_COMBAT_FEEDBACK_NUTSHOT:
            return "NUT SHOT";
        case FT_COMBAT_FEEDBACK_FIRSTKILL:
            return "FIRST KILL";
        case FT_COMBAT_FEEDBACK_DOUBLEKILL:
            return "DOUBLE KILL";
        case FT_COMBAT_FEEDBACK_MULTIKILL:
            return "MULTI KILL";
        case FT_COMBAT_FEEDBACK_ULTRAKILL:
            return "ULTRA KILL";
        case FT_COMBAT_FEEDBACK_FANTASTIC:
            return "FANTASTIC";
        case FT_COMBAT_FEEDBACK_UNBELIEVABLE:
            return "UNBELIEVABLE";
        case FT_FEEDBACK_ROUNDSTART:
            return "ROUND START";
        default:
            return "";
    }
}

static void FT_PlayFeedbackCue(
    const char *pPath,
    uint8 nVolume)
{
    if(!g_pLTCSoundMgr ||
       !pPath ||
       !pPath[0])
    {
        return;
    }

    PlaySoundInfo info;
    PLAYSOUNDINFO_INIT(info);
    info.m_dwFlags =
        PLAYSOUND_LOCAL |
        PLAYSOUND_CTRL_VOL;
    info.m_nVolume = nVolume;

    strncpy(
        info.m_szSoundName,
        pPath,
        sizeof(info.m_szSoundName) - 1);
    info.m_szSoundName[
        sizeof(info.m_szSoundName) - 1] =
        '\0';

    HLTSOUND hSound = LTNULL;
    g_pLTCSoundMgr->PlaySound(
        &info,
        hSound);
}

static void FT_StartFeedback(
    uint8 nFeedback)
{
    s_nActiveFeedback = nFeedback;
    s_fActiveStart =
        g_pLTClient
        ? g_pLTClient->GetTime()
        : 0.0f;
}

static void FT_QueueFeedback(
    uint8 nFeedback)
{
    if(!nFeedback)
        return;

    if(!s_nActiveFeedback)
    {
        FT_StartFeedback(nFeedback);
        return;
    }

    if(s_nQueueCount >=
       FT_FEEDBACK_QUEUE_SIZE)
    {
        return;
    }

    const uint8 nTail =
        (uint8)(
            (s_nQueueHead +
             s_nQueueCount) %
            FT_FEEDBACK_QUEUE_SIZE);

    s_aQueue[nTail] = nFeedback;
    ++s_nQueueCount;
}

static void FT_AdvanceFeedback()
{
    if(!s_nQueueCount)
    {
        s_nActiveFeedback = 0;
        s_fActiveStart = 0.0f;
        return;
    }

    const uint8 nNext =
        s_aQueue[s_nQueueHead];

    s_nQueueHead =
        (uint8)(
            (s_nQueueHead + 1) %
            FT_FEEDBACK_QUEUE_SIZE);
    --s_nQueueCount;

    FT_StartFeedback(nNext);
}

static float FT_BaseWidth(
    uint8 nFeedback,
    uint32 nScreenW)
{
    float fWidth =
        (float)nScreenW * 0.46f;
    float fMax = 780.0f;

    if(nFeedback ==
           FT_COMBAT_FEEDBACK_HEADSHOT ||
       nFeedback ==
           FT_COMBAT_FEEDBACK_NUTSHOT)
    {
        fMax = 690.0f;
    }
    else if(nFeedback ==
            FT_FEEDBACK_ROUNDSTART)
    {
        fMax = 720.0f;
    }

    if(fWidth > fMax)
        fWidth = fMax;
    if(fWidth < 380.0f)
        fWidth = 380.0f;

    return fWidth;
}

static void FT_DrawImage(
    FTFeedbackImage *pImage,
    float fCenterY,
    float fWidth,
    uint8 nAlpha)
{
    if(!pImage ||
       !pImage->hTexture ||
       !pImage->nWidth ||
       !pImage->nHeight)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    const float fHeight =
        fWidth *
        ((float)pImage->nHeight /
         (float)pImage->nWidth);
    const float fLeft =
        ((float)nScreenW - fWidth) *
        0.5f;
    const float fTop =
        fCenterY -
        (fHeight * 0.5f);

    LT_POLYFT4 poly;

    poly.verts[0].x = fLeft;
    poly.verts[0].y = fTop;
    poly.verts[0].z = SCREEN_NEAR_Z;
    poly.verts[0].u = 0.0f;
    poly.verts[0].v = 0.0f;

    poly.verts[1].x = fLeft + fWidth;
    poly.verts[1].y = fTop;
    poly.verts[1].z = SCREEN_NEAR_Z;
    poly.verts[1].u = 1.0f;
    poly.verts[1].v = 0.0f;

    poly.verts[2].x = fLeft + fWidth;
    poly.verts[2].y = fTop + fHeight;
    poly.verts[2].z = SCREEN_NEAR_Z;
    poly.verts[2].u = 1.0f;
    poly.verts[2].v = 1.0f;

    poly.verts[3].x = fLeft;
    poly.verts[3].y = fTop + fHeight;
    poly.verts[3].z = SCREEN_NEAR_Z;
    poly.verts[3].u = 0.0f;
    poly.verts[3].v = 1.0f;

    poly.rgba.r = 255;
    poly.rgba.g = 255;
    poly.rgba.b = 255;
    poly.rgba.a = nAlpha;

    g_pLTCDrawPrim->SetTexture(
        pImage->hTexture);
    g_pLTCDrawPrim->SetTransformType(
        DRAWPRIM_TRANSFORM_SCREEN);
    g_pLTCDrawPrim->SetColorOp(
        DRAWPRIM_MODULATE);
    g_pLTCDrawPrim->SetAlphaBlendMode(
        DRAWPRIM_BLEND_MOD_SRCALPHA);
    g_pLTCDrawPrim->SetZBufferMode(
        DRAWPRIM_NOZ);
    g_pLTCDrawPrim->SetAlphaTestMode(
        DRAWPRIM_NOALPHATEST);
    g_pLTCDrawPrim->SetClipMode(
        DRAWPRIM_FASTCLIP);
    g_pLTCDrawPrim->SetFillMode(
        DRAWPRIM_FILL);
    g_pLTCDrawPrim->SetCullMode(
        DRAWPRIM_CULL_NONE);
    g_pLTCDrawPrim->SetCamera(
        LTNULL);

    g_pLTCDrawPrim->BeginDrawPrim();
    g_pLTCDrawPrim->DrawPrim(
        &poly,
        1);
    g_pLTCDrawPrim->EndDrawPrim();
    g_pLTCDrawPrim->SetTexture(
        LTNULL);
}

static void FT_DrawFallback(
    uint8 nFeedback,
    float fCenterY,
    uint8 nAlpha)
{
    if(!s_pFallback)
        return;

    const char *pLabel =
        FT_TextForFeedback(nFeedback);

    if(!pLabel || !pLabel[0])
        return;

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    const uint32 nRgb =
        (nFeedback ==
             FT_COMBAT_FEEDBACK_HEADSHOT ||
         nFeedback ==
             FT_COMBAT_FEEDBACK_NUTSHOT)
        ? 0x00FFB000
        : 0x00FFFFFF;

    s_pFallback->SetColor(
        ((uint32)nAlpha << 24) |
        nRgb);
    s_pFallback->SetText(pLabel);
    s_pFallback->SetPosition(
        ((float)nScreenW -
         s_pFallback->GetWidth()) *
            0.5f,
        fCenterY -
        (s_pFallback->GetHeight() *
         0.5f));
    s_pFallback->Render();
}

void FT_CombatFeedbackInit()
{
    FT_LoadFeedbackImage(
        s_Headshot,
        "UI_HUD_MESSAGE/EFFECT/HEADSHOT.DTX");
    FT_LoadFeedbackImage(
        s_Nutshot,
        "UI_HUD_MESSAGE/EFFECT/NUTSHOT.DTX");
    FT_LoadFeedbackImage(
        s_FirstKill,
        "UI_HUD_MESSAGE/EFFECT/FIRSTKILL.DTX");
    FT_LoadFeedbackImage(
        s_DoubleKill,
        "UI_HUD_MESSAGE/EFFECT/DOUBLEKILL.DTX");
    FT_LoadFeedbackImage(
        s_MultiKill,
        "UI_HUD_MESSAGE/EFFECT/MULTIKILL.DTX");
    FT_LoadFeedbackImage(
        s_UltraKill,
        "UI_HUD_MESSAGE/EFFECT/ULTRAKILL.DTX");
    FT_LoadFeedbackImage(
        s_Fantastic,
        "UI_HUD_MESSAGE/EFFECT/FANTASTIC.DTX");
    FT_LoadFeedbackImage(
        s_Unbelievable,
        "UI_HUD_MESSAGE/EFFECT/UNBELIEVABLE.DTX");
    FT_LoadFeedbackImage(
        s_RoundStart,
        "UI_HUD_MESSAGE/EFFECT/ROUNDSTART.DTX");

    if(!s_pFeedbackFont &&
       g_pLTCFontManager)
    {
        s_pFeedbackFont =
            g_pLTCFontManager->CreateFont(
                "fonts/SQR721B.TTF",
                "Square721 BT",
                42,
                64,
                255);

        if(s_pFeedbackFont)
        {
            s_pFeedbackFont->SetDefCharWidth(
                11);
            s_pFeedbackFont->SetDefColor(
                0xFFFFFFFF);

            s_pFallback =
                g_pLTCFontManager->CreateFormattedPolyString(
                    s_pFeedbackFont,
                    "");
        }
    }

    s_nQueueHead = 0;
    s_nQueueCount = 0;
    s_nActiveFeedback = 0;
    s_fActiveStart = 0.0f;
}

void FT_CombatFeedbackTerm()
{
    FT_ReleaseFeedbackImage(s_Headshot);
    FT_ReleaseFeedbackImage(s_Nutshot);
    FT_ReleaseFeedbackImage(s_FirstKill);
    FT_ReleaseFeedbackImage(s_DoubleKill);
    FT_ReleaseFeedbackImage(s_MultiKill);
    FT_ReleaseFeedbackImage(s_UltraKill);
    FT_ReleaseFeedbackImage(s_Fantastic);
    FT_ReleaseFeedbackImage(s_Unbelievable);
    FT_ReleaseFeedbackImage(s_RoundStart);

    if(s_pFallback)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pFallback);
        s_pFallback = LTNULL;
    }

    if(s_pFeedbackFont)
    {
        g_pLTCFontManager->DestroyFont(
            s_pFeedbackFont);
        s_pFeedbackFont = LTNULL;
    }

    s_nQueueHead = 0;
    s_nQueueCount = 0;
    s_nActiveFeedback = 0;
    s_fActiveStart = 0.0f;
}

void FT_CombatFeedbackHandleMessage(
    ILTMessage_Read *pMessage)
{
    if(!pMessage)
        return;

    const uint8 nFeedback =
        pMessage->Readuint8();

    FT_QueueFeedback(nFeedback);

    // The supplied CA SND archive does not contain the literal announcer VO.
    // Keep the native Training target cues as restrained kill confirmation.
    if(nFeedback ==
       FT_COMBAT_FEEDBACK_HEADSHOT)
    {
        FT_PlayFeedbackCue(
            "Snd/TRAINING/TARGET_DOWN1.WAV",
            62);
    }
    else if(nFeedback ==
            FT_COMBAT_FEEDBACK_NUTSHOT)
    {
        FT_PlayFeedbackCue(
            "Snd/TRAINING/TARGET_UP1.WAV",
            62);
    }
}

void FT_CombatFeedbackShowRoundStart()
{
    FT_QueueFeedback(
        FT_FEEDBACK_ROUNDSTART);
}

void FT_RenderCombatFeedback()
{
    if(!g_pLTClient ||
       !g_pLTCDrawPrim ||
       !s_nActiveFeedback)
    {
        return;
    }

    const float fAge =
        g_pLTClient->GetTime() -
        s_fActiveStart;

    const float kZoomSeconds = 0.18f;
    const float kHoldUntil = 1.45f;
    const float kTotalSeconds = 2.10f;

    if(fAge >= kTotalSeconds)
    {
        FT_AdvanceFeedback();
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    float fScale = 1.0f;

    if(fAge < kZoomSeconds)
    {
        const float fProgress =
            fAge / kZoomSeconds;

        // Original-style impact: oversized first frame, quickly settling down.
        fScale =
            1.45f -
            (0.45f * fProgress);
    }

    uint8 nAlpha = 255;

    if(fAge > kHoldUntil)
    {
        float fFade =
            1.0f -
            ((fAge - kHoldUntil) /
             (kTotalSeconds - kHoldUntil));

        if(fFade < 0.0f)
            fFade = 0.0f;

        nAlpha =
            (uint8)(fFade * 255.0f);
    }

    const float fWidth =
        FT_BaseWidth(
            s_nActiveFeedback,
            nScreenW) *
        fScale;
    const float fCenterY =
        (float)nScreenH * 0.39f;

    FTFeedbackImage *pImage =
        FT_ImageForFeedback(
            s_nActiveFeedback);

    if(pImage &&
       pImage->hTexture)
    {
        FT_DrawImage(
            pImage,
            fCenterY,
            fWidth,
            nAlpha);
    }
    else
    {
        FT_DrawFallback(
            s_nActiveFeedback,
            fCenterY,
            nAlpha);
    }
}
