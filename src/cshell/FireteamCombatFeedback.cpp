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

static FTFeedbackImage s_Headshot = { LTNULL, 0, 0 };
static FTFeedbackImage s_Nutshot = { LTNULL, 0, 0 };
static FTFeedbackImage s_FirstKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_DoubleKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_MultiKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_UltraKill = { LTNULL, 0, 0 };
static FTFeedbackImage s_Fantastic = { LTNULL, 0, 0 };
static FTFeedbackImage s_Unbelievable = { LTNULL, 0, 0 };

static CUIFont *s_pFeedbackFont = LTNULL;
static CUIFormattedPolyString *s_pRegionFallback = LTNULL;
static CUIFormattedPolyString *s_pStreakFallback = LTNULL;

static uint8 s_nRegionFeedback = 0;
static uint8 s_nStreakFeedback = 0;
static float s_fRegionUntil = 0.0f;
static float s_fStreakUntil = 0.0f;

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

    PlaySoundInfo soundInfo;
    PLAYSOUNDINFO_INIT(
        soundInfo);

    soundInfo.m_dwFlags =
        PLAYSOUND_LOCAL |
        PLAYSOUND_CTRL_VOL;
    soundInfo.m_nVolume =
        nVolume;

    strncpy(
        soundInfo.m_szSoundName,
        pPath,
        sizeof(soundInfo.m_szSoundName) - 1);
    soundInfo.m_szSoundName[
        sizeof(soundInfo.m_szSoundName) - 1] =
        '\0';

    HLTSOUND hSound =
        LTNULL;

    g_pLTCSoundMgr->PlaySound(
        &soundInfo,
        hSound);
}

static void FT_DrawFeedbackImage(
    FTFeedbackImage *pImage,
    float fCenterY,
    float fMaxWidth)
{
    if(!pImage ||
       !pImage->hTexture ||
       pImage->nWidth == 0 ||
       pImage->nHeight == 0)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    float fWidth =
        (float)nScreenW * 0.30f;

    if(fWidth > fMaxWidth)
        fWidth = fMaxWidth;

    if(fWidth < 220.0f)
        fWidth = 220.0f;

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
    poly.rgba.a = 255;

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
    CUIFormattedPolyString *pText,
    uint8 nFeedback,
    float fCenterY)
{
    if(!pText)
    {
        return;
    }

    const char *pLabel =
        FT_TextForFeedback(
            nFeedback);

    if(!pLabel ||
       !pLabel[0])
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    pText->SetText(
        pLabel);
    pText->SetPosition(
        ((float)nScreenW -
         pText->GetWidth()) *
            0.5f,
        fCenterY -
        (pText->GetHeight() *
         0.5f));
    pText->Render();
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

    if(!s_pFeedbackFont &&
       g_pLTCFontManager)
    {
        s_pFeedbackFont =
            g_pLTCFontManager->CreateFont(
                "fonts/SQR721B.TTF",
                "Square721 BT",
                28,
                44,
                255);

        if(s_pFeedbackFont)
        {
            s_pFeedbackFont->SetDefCharWidth(
                8);
            s_pFeedbackFont->SetDefColor(
                0xFFFFB000);

            s_pRegionFallback =
                g_pLTCFontManager->CreateFormattedPolyString(
                    s_pFeedbackFont,
                    "");
            s_pStreakFallback =
                g_pLTCFontManager->CreateFormattedPolyString(
                    s_pFeedbackFont,
                    "");

            if(s_pRegionFallback)
                s_pRegionFallback->SetColor(
                    0xFFFFB000);
            if(s_pStreakFallback)
                s_pStreakFallback->SetColor(
                    0xFFFFFFFF);
        }
    }
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

    if(s_pRegionFallback)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pRegionFallback);
        s_pRegionFallback = LTNULL;
    }

    if(s_pStreakFallback)
    {
        g_pLTCFontManager->DestroyPolyString(
            s_pStreakFallback);
        s_pStreakFallback = LTNULL;
    }

    if(s_pFeedbackFont)
    {
        g_pLTCFontManager->DestroyFont(
            s_pFeedbackFont);
        s_pFeedbackFont = LTNULL;
    }

    s_nRegionFeedback = 0;
    s_nStreakFeedback = 0;
    s_fRegionUntil = 0.0f;
    s_fStreakUntil = 0.0f;
}

void FT_CombatFeedbackHandleMessage(
    ILTMessage_Read *pMessage)
{
    if(!pMessage)
    {
        return;
    }

    const uint8 nFeedback =
        pMessage->Readuint8();
    const float fNow =
        g_pLTClient->GetTime();

    if(nFeedback ==
           FT_COMBAT_FEEDBACK_HEADSHOT ||
       nFeedback ==
           FT_COMBAT_FEEDBACK_NUTSHOT)
    {
        s_nRegionFeedback =
            nFeedback;
        s_fRegionUntil =
            fNow + 1.20f;

        // No literal HEADSHOT/NUTSHOT announcer VO exists in the supplied
        // SND archive. These are short native CA target cues, used as a
        // restrained local hit-confirm layer rather than fake announcer VO.
        FT_PlayFeedbackCue(
            nFeedback ==
                FT_COMBAT_FEEDBACK_HEADSHOT
            ? "Snd/TRAINING/TARGET_DOWN1.WAV"
            : "Snd/TRAINING/TARGET_UP1.WAV",
            62);
    }
    else
    {
        s_nStreakFeedback =
            nFeedback;
        s_fStreakUntil =
            fNow + 1.65f;
    }
}

void FT_RenderCombatFeedback()
{
    if(!g_pLTClient ||
       !g_pLTCDrawPrim)
    {
        return;
    }

    const float fNow =
        g_pLTClient->GetTime();

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    if(s_nRegionFeedback &&
       fNow < s_fRegionUntil)
    {
        FTFeedbackImage *pImage =
            FT_ImageForFeedback(
                s_nRegionFeedback);

        if(pImage &&
           pImage->hTexture)
        {
            FT_DrawFeedbackImage(
                pImage,
                (float)nScreenH * 0.37f,
                340.0f);
        }
        else
        {
            FT_DrawFallback(
                s_pRegionFallback,
                s_nRegionFeedback,
                (float)nScreenH * 0.37f);
        }
    }

    if(s_nStreakFeedback &&
       fNow < s_fStreakUntil)
    {
        FTFeedbackImage *pImage =
            FT_ImageForFeedback(
                s_nStreakFeedback);

        if(pImage &&
           pImage->hTexture)
        {
            FT_DrawFeedbackImage(
                pImage,
                (float)nScreenH * 0.50f,
                390.0f);
        }
        else
        {
            FT_DrawFallback(
                s_pStreakFallback,
                s_nStreakFeedback,
                (float)nScreenH * 0.50f);
        }
    }
}
