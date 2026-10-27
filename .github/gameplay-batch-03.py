edit('src/cshell/FireteamHealthHud.cpp', 'c41e7acf10f5cec5435c2b46db3dd0e3c95df7ee43f6e1073c0c49e76d29d1e3', [
    (61, 62, r"""#include "FireteamPresentationRules.h"
"""),
    (110, 110, r"""#include <iltfontmanager.h>
#include <stdio.h>
#include <math.h>
"""),
    (111, 174, r"""static uint8 s_nHealth = 100, s_nMaxHealth = 100;
static float s_fDamageUntil = 0.0f;
static CUIFont *s_pHealthFont = LTNULL;
static CUIFormattedPolyString *s_pHealthText = LTNULL;
struct FTDamageCue { LTVector source; float until; };
static FTDamageCue s_aDamage[4];
static unsigned s_nNextDamage = 0;
"""),
    (175, 317, r"""static float FT_HealthOption(const char *name, float fallback)
"""),
    (319, 789, r"""    HCONSOLEVAR h = g_pLTClient->GetConsoleVar(name);
    return h ? g_pLTClient->GetVarValueFloat(h) : fallback;
"""),
    (791, 843, r"""static void FT_HealthRect(LT_POLYF4 &p, float x, float y, float w, float h,
    uint8 red, uint8 green, uint8 blue, uint8 alpha)
"""),
    (845, 960, r"""    p.rgba.r = red; p.rgba.g = green; p.rgba.b = blue; p.rgba.a = alpha;
    p.verts[0].x=x; p.verts[0].y=y;
    p.verts[1].x=x+w; p.verts[1].y=y;
    p.verts[2].x=x+w; p.verts[2].y=y+h;
    p.verts[3].x=x; p.verts[3].y=y+h;
    for(unsigned i=0;i<4;++i) p.verts[i].z=SCREEN_NEAR_Z;
"""),
    (962, 963, r"""void FT_HealthHudReset()
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
"""),
    (1012, 1247, r"""    // Original map-specific PoisonGas semantics remain intentionally disabled.
"""),
    (1268, 1268, r"""void FT_RenderHealthHud(HOBJECT hCamera)
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
"""),
    (1269, 1949, r"""    g_pLTCDrawPrim->SetCamera(LTNULL);
"""),
    (2437, 2477, r""""""),
    (2514, 2582, r"""    LT_POLYF4 p;
    // A local bar flash, not a full-screen red filter or camera shake.
    const bool hurt = feedback && !reduced && now < s_fDamageUntil;
    FT_HealthRect(p,x-2,y-2,width+4,20, hurt ? 240 : 50, hurt ? 100 : 50,40,235);
    g_pLTCDrawPrim->DrawPrim(&p,1);
    FT_HealthRect(p,x,y,width,16,20,20,20,240);
    g_pLTCDrawPrim->DrawPrim(&p,1);
    if(s_nHealth)
"""),
    (2588, 2634, r"""        FT_HealthRect(p,x+2,y+2,(width-4)*ratio,12,critical ? 245 : 210,
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
"""),
    (2675, 2676, r"""    g_pLTCDrawPrim->SetColorOp(DRAWPRIM_MODULATE);
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
"""),
])
edit('src/cshell/FireteamHealthHud.h', '15919842e71605a913434516caeb9fb244a52e1a5c106c4d2f1b72ef53957b38', [
    (68, 69, r""""""),
    (93, 94, r"""void FT_HealthHudInit();
void FT_HealthHudTerm();
void FT_HealthHudReset();
"""),
    (146, 146, r"""void FT_HealthHudDamage(const LTVector &vSource, uint8 nDamage);
"""),
    (194, 228, r"""void FT_RenderHealthHud(HOBJECT hCamera);
#endif
"""),
])
edit('src/cshell/FireteamRadarHud.cpp', '4b5cc04c6e7d66386acbc524b5e15b4ab66203a50973de8fc10ea3427e9b4c34', [
    (60, 60, r"""#include "FireteamPresentationRules.h"
"""),
    (3022, 3144, r"""    HCONSOLEVAR hScale = g_pLTClient->GetConsoleVar("ftradarscale");
    const float radius = FTPresentationRules::RadarRadius((float)screenH,
        hScale ? g_pLTClient->GetVarValueFloat(hScale) : 1.0f);
"""),
    (6299, 6299, r"""            if(dot.nType == 3)
            {
                LT_POLYF4 center = FT_RadarRect(cx+dx-1, cy-dy-1, 2, 2, 16, 25, 30, 255);
                g_pLTCDrawPrim->DrawPrim(&center,1);
            }
            else if(dot.nType == 1)
            {
                LT_POLYF4 cross = FT_RadarRect(cx+dx-0.8f,cy-dy-4,1.6f,8,235,255,250,235);
                g_pLTCDrawPrim->DrawPrim(&cross,1);
            }
"""),
])
edit('src/cshell/FireteamRoundHud.cpp', '777a3f1b313559b1b6ed2b384da556e86a48dcc208938060ed9cd533008065a0', [
    (96, 96, r"""#include "FireteamPresentationRules.h"
"""),
    (876, 876, r"""static CUIFormattedPolyString *s_pWaveDetail = LTNULL;
static float s_fIntermissionUntil = 0.0f;
static uint16 s_nIntermissionRound = 0;
"""),
    (3931, 3931, r"""        s_pWaveDetail = g_pLTCFontManager->CreateFormattedPolyString(s_pBuffFont, "");
"""),
    (5445, 5445, r"""    if(s_pWaveDetail) g_pLTCFontManager->DestroyPolyString(s_pWaveDetail);
    s_pWaveDetail = LTNULL;
    s_fIntermissionUntil = 0.0f;
    s_nIntermissionRound = 0;
"""),
    (7645, 7645, r"""void FT_RoundHudSetIntermission(uint16 round, float remaining)
{
    if(round < s_nRound || !g_pLTClient) return;
    s_nIntermissionRound = round;
    const float seconds = FTCombatRules::Clamp(remaining, 0, 120, 0);
    s_fIntermissionUntil = seconds > 0 ? g_pLTClient->GetTime() + seconds : 0;
}

"""),
    (8128, 8128, r"""    if(bCombatActive || nState == 3) s_fIntermissionUntil = 0.0f;
"""),
    (10573, 10573, r"""    s_fIntermissionUntil = 0.0f;
    s_nIntermissionRound = 0;
"""),
    (13830, 13876, r"""    if(s_pRoundStatus && s_nRound > 0)
"""),
    (13882, 14423, r"""        const unsigned left = s_nTarget > s_nKilled ? s_nTarget - s_nKilled : 0;
        const unsigned incoming = FTCombatRules::PendingSpawns(s_nTarget, s_nKilled, s_nAlive);
        HCONSOLEVAR hRadarScale = g_pLTClient->GetConsoleVar("ftradarscale");
        const float radarScale = hRadarScale ? g_pLTClient->GetVarValueFloat(hRadarScale) : 1.0f;
        const float safeWidth = (float)nScreenW -
            2.0f * FTPresentationRules::RadarRadius((float)nScreenH, radarScale) - 72.0f;
        char status[96];
        sprintf(status, "ROUND %u   LEFT %u   ALIVE %u", (unsigned)s_nRound, left, (unsigned)s_nAlive);
        s_pRoundStatus->SetText(status);
        if(s_pRoundStatus->GetWidth() > safeWidth)
        {
            sprintf(status, "R%u   LEFT %u   UP %u", (unsigned)s_nRound, left, (unsigned)s_nAlive);
            s_pRoundStatus->SetText(status);
        }
        const float statusX = 24.0f + (safeWidth - s_pRoundStatus->GetWidth()) * 0.5f;
        s_pRoundStatus->SetPosition(statusX > 18.0f ? statusX : 18.0f, 22.0f);
"""),
    (14457, 14457, r"""        if(s_pWaveDetail && !s_bSpectating && !s_bGameOver)
        {
            if(!s_bCombatTimerActive && s_nIntermissionRound == s_nRound && s_fIntermissionUntil > 0)
            {
                const unsigned seconds = FTPresentationRules::Countdown(
                    s_fIntermissionUntil, g_pLTClient->GetTime());
                if(seconds) sprintf(status, "NEXT ROUND IN %us  |  RELOAD AND REGROUP", seconds);
                else sprintf(status, "NEXT WAVE STARTING...");
            }
            else sprintf(status, "INCOMING %u   KILLS %u/%u   LIVES %u/%u", incoming,
                (unsigned)s_nKilled, (unsigned)s_nTarget, (unsigned)s_nLives, (unsigned)s_nMaxLives);
            s_pWaveDetail->SetText(status);
            const float detailX = 24.0f + (safeWidth - s_pWaveDetail->GetWidth()) * 0.5f;
            s_pWaveDetail->SetPosition(detailX > 18.0f ? detailX : 18.0f, 55.0f);
            s_pWaveDetail->Render();
        }
"""),
])
edit('src/cshell/FireteamRoundHud.h', '2ab2165b62a8212dbd426cedeb43521e97e81605644b743adf67a56284bf2376', [
    (222, 222, r"""void FT_RoundHudSetIntermission(uint16 nRound, float fRemaining);
"""),
])
