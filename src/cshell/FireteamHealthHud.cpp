#include "FireteamHealthHud.h"
#include "clientinterfaces.h"
#include "FireteamPresentationRules.h"
#include <iltclient.h>
#include <iltdrawprim.h>
#include <iltfontmanager.h>
#include <stdio.h>
#include <math.h>

static uint8 s_nHealth = 100, s_nMaxHealth = 100;
static float s_fDamageUntil = 0.0f;
static CUIFont *s_pHealthFont = LTNULL;
static CUIFormattedPolyString *s_pHealthText = LTNULL;
struct FTDamageCue { LTVector source; float until; };
static FTDamageCue s_aDamage[4];
static unsigned s_nNextDamage = 0;

static float FT_HealthOption(const char *name, float fallback)
{
    HCONSOLEVAR h = g_pLTClient->GetConsoleVar(name);
    return h ? g_pLTClient->GetVarValueFloat(h) : fallback;
}
static void FT_HealthRect(LT_POLYF4 &p, float x, float y, float w, float h,
    uint8 red, uint8 green, uint8 blue, uint8 alpha)
{
    p.rgba.r = red; p.rgba.g = green; p.rgba.b = blue; p.rgba.a = alpha;
    p.verts[0].x=x; p.verts[0].y=y;
    p.verts[1].x=x+w; p.verts[1].y=y;
    p.verts[2].x=x+w; p.verts[2].y=y+h;
    p.verts[3].x=x; p.verts[3].y=y+h;
    for(unsigned i=0;i<4;++i) p.verts[i].z=SCREEN_NEAR_Z;
}
void FT_HealthHudReset()
{
    s_nHealth = s_nMaxHealth = 100;
    s_fDamageUntil = 0;
    s_nNextDamage = 0;
    for(unsigned i=0;i<4;++i) s_aDamage[i].until=0;
}
void FT_HealthHudInit()
{
    if(s_pHealthFont || !g_pLTCFontManager) return;
    s_pHealthFont = g_pLTCFontManager->CreateFont(
        "fonts/SQR721B.TTF", "Square721 BT", 18, 30, 255);
    if(!s_pHealthFont) return;
    s_pHealthFont->SetDefCharWidth(6);
    s_pHealthFont->SetDefColor(0xFFFFFFFF);
    s_pHealthText = g_pLTCFontManager->CreateFormattedPolyString(s_pHealthFont, "");
}
void FT_HealthHudTerm()
{
    if(s_pHealthText) g_pLTCFontManager->DestroyPolyString(s_pHealthText);
    if(s_pHealthFont) g_pLTCFontManager->DestroyFont(s_pHealthFont);
    s_pHealthText = LTNULL; s_pHealthFont = LTNULL;
    FT_HealthHudReset();
}
void FT_SetHealth(uint8 health, uint8 maximum)
{
    if(health < s_nHealth && g_pLTClient)
        s_fDamageUntil = g_pLTClient->GetTime() + 0.6f;
    s_nMaxHealth = maximum ? maximum : 1;
    s_nHealth = health > s_nMaxHealth ? s_nMaxHealth : health;
    if(s_nHealth == 0)
        for(unsigned i=0;i<4;++i) s_aDamage[i].until=0;
}
void FT_HealthHudDamage(const LTVector &source, uint8 damage)
{
    if(!g_pLTClient || !damage || !s_nHealth ||
       !FTCombatRules::Finite(source.x) || !FTCombatRules::Finite(source.y) ||
       !FTCombatRules::Finite(source.z)) return;
    FTDamageCue &cue = s_aDamage[s_nNextDamage++ % 4];
    cue.source = source;
    cue.until = g_pLTClient->GetTime() + 0.8f;
}
void FT_RenderPoisonOverlay(HLOCALOBJ hPlayer)
{
    // Original map-specific PoisonGas semantics remain intentionally disabled.
    (void)hPlayer;
}
void FT_RenderHealthHud(HOBJECT hCamera)
{
    if(!g_pLTClient || !g_pLTCDrawPrim) return;
    uint32 screenW=0, screenH=0;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &screenW, &screenH);
    if(screenW < 320 || screenH < 240) return;
    const float now = g_pLTClient->GetTime();
    const float width = screenW < 640 ? 160.0f : 240.0f;
    const float x=28.0f, y=(float)screenH-42.0f;
    const float ratio = (float)s_nHealth / s_nMaxHealth;
    const bool critical = s_nHealth > 0 && ratio <= 0.25f;
    const bool feedback = FT_HealthOption("ftdamagefeedback", 1) >= 0.5f;
    const bool reduced = FT_HealthOption("ftreducedfx", 0) >= 0.5f;

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
    LT_POLYF4 p;
    // A local bar flash, not a full-screen red filter or camera shake.
    const bool hurt = feedback && !reduced && now < s_fDamageUntil;
    FT_HealthRect(p,x-2,y-2,width+4,20, hurt ? 240 : 50, hurt ? 100 : 50,40,235);
    g_pLTCDrawPrim->DrawPrim(&p,1);
    FT_HealthRect(p,x,y,width,16,20,20,20,240);
    g_pLTCDrawPrim->DrawPrim(&p,1);
    if(s_nHealth)
    {
        FT_HealthRect(p,x+2,y+2,(width-4)*ratio,12,critical ? 245 : 210,
            critical ? 130 : 36,36,255);
        g_pLTCDrawPrim->DrawPrim(&p,1);
    }
    if(feedback && hCamera && s_nHealth)
    {
        LTVector cameraPos; LTRotation cameraRot;
        if(g_pLTClient->GetObjectPos(hCamera,&cameraPos)==LT_OK &&
           g_pLTClient->GetObjectRotation(hCamera,&cameraRot)==LT_OK)
        {
            LTVector forward=cameraRot.Forward(); forward.y=0;
            if(forward.MagSqr()>0.0001f)
            {
                forward.Normalize();
                LTVector right=cameraRot.Right(); right.y=0;
                if(right.MagSqr()>0.0001f) right.Normalize();
                uint8 alpha[4]={0,0,0,0};
                for(unsigned i=0;i<4;++i)
                {
                    const float left=s_aDamage[i].until-now;
                    if(left<=0 || left>0.8f) continue;
                    LTVector offset=s_aDamage[i].source-cameraPos; offset.y=0;
                    const int side=FTPresentationRules::DamageSide(offset.Dot(right),offset.Dot(forward));
                    if(side<0) continue;
                    const uint8 a=(uint8)(reduced ? 175.0f : 220.0f*left/0.8f);
                    if(a>alpha[side]) alpha[side]=a;
                }
                for(unsigned i=0;i<4;++i) if(alpha[i])
                {
                    const float w=(i==1 || i==3) ? 5.0f : 100.0f;
                    const float h=(i==1 || i==3) ? 100.0f : 5.0f;
                    const float px=i==1 ? (float)screenW-17.0f : i==3 ? 12.0f : ((float)screenW-w)*0.5f;
                    const float py=i==0 ? 12.0f : i==2 ? (float)screenH-17.0f : ((float)screenH-h)*0.5f;
                    FT_HealthRect(p,px,py,w,h,255,165,70,alpha[i]);
                    g_pLTCDrawPrim->DrawPrim(&p,1);
                }
            }
        }
    }
    g_pLTCDrawPrim->EndDrawPrim();
    g_pLTCDrawPrim->SetColorOp(DRAWPRIM_MODULATE);
    if(s_pHealthText)
    {
        char label[64];
        sprintf(label,"HP %u / %u%s",(unsigned)s_nHealth,(unsigned)s_nMaxHealth,
            critical ? "  LOW HEALTH" : "");
        s_pHealthText->SetText(label);
        s_pHealthText->SetColor(critical ? 0xFFFFC070 : 0xFFFFFFFF);
        s_pHealthText->SetPosition(x,y-27.0f);
        s_pHealthText->Render();
    }
}
