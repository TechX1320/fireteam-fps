edit('src/cshell/FireteamSettingsMenu.cpp', '8dececa431c098f47b20cc7be7b7cfd0bd7fe3cae5bd2d22bcd56985565af8d8', [
    (64, 64, r"""#include "FireteamCombatRules.h"
"""),
    (3084, 3140, r"""    char szTemporary[96];
    sprintf(szTemporary, "fireteam-settings.cfg.%lu.tmp", (unsigned long)GetCurrentProcessId());
    FILE *pFile = fopen(szTemporary, "wt");
"""),
    (3212, 3252, r"""        "%.6f %.6f %u %.3f %u %u %u %u %.3f %u %u\n",
"""),
    (3437, 3467, r"""        s_bWindowed ? 1 : 0,
        FTGetConsoleFloat("ftradarenabled", 1) >= 0.5f ? 1u : 0u,
        FTCombatRules::Clamp(FTGetConsoleFloat("ftradarscale", 1), 0.75f, 1.75f, 1),
        FTGetConsoleFloat("ftdamagefeedback", 1) >= 0.5f ? 1u : 0u,
        FTGetConsoleFloat("ftreducedfx", 0) >= 0.5f ? 1u : 0u);
"""),
    (3468, 3487, r"""    const bool written = !ferror(pFile);
    const int closed = fclose(pFile);
    if(written && closed == 0)
        MoveFileExA(szTemporary, "fireteam-settings.cfg",
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    DeleteFileA(szTemporary); // No-op after successful atomic rename.
"""),
    (3971, 3971, r"""    uint32 nRadar = 1, nDamage = 1, nReduced = 0;
    float fRadarScale = 1.0f;
"""),
    (4012, 4044, r"""        "%f %f %u %f %u %u %u %u %f %u %u",
"""),
    (4167, 4188, r"""        &nWindowed, &nRadar, &fRadarScale, &nDamage, &nReduced);
"""),
    (4189, 4208, r"""    if(nRead < 7)
"""),
    (4569, 4588, r"""    if(nRead < 7)
"""),
    (4984, 5003, r"""    if(nRead < 7)
"""),
    (5508, 5527, r"""    if(nRead >= 7)
"""),
    (6375, 6375, r"""        // Explicit launch-console overrides win. Direct EXE starts load the
        // persisted fields without resetting the launcher's radar choices.
        char option[80];
        if(!g_pLTClient->GetConsoleVar("ftradarenabled"))
        { sprintf(option,"ftradarenabled %u",nRadar ? 1u : 0u); g_pLTClient->RunConsoleString(option); }
        if(!g_pLTClient->GetConsoleVar("ftradarscale"))
        { sprintf(option,"ftradarscale %.3f",FTCombatRules::Clamp(fRadarScale,0.75f,1.75f,1)); g_pLTClient->RunConsoleString(option); }
        if(!g_pLTClient->GetConsoleVar("ftdamagefeedback"))
        { sprintf(option,"ftdamagefeedback %u",nDamage ? 1u : 0u); g_pLTClient->RunConsoleString(option); }
        if(!g_pLTClient->GetConsoleVar("ftreducedfx"))
        { sprintf(option,"ftreducedfx %u",nReduced ? 1u : 0u); g_pLTClient->RunConsoleString(option); }
"""),
])
edit('src/cshell/FireteamWeaponHud.cpp', '1f2bc69bc9ea7b53dbb0a5a2fc13f3f01e1e3242e3cfdf6bed93d97ee98b7489', [
    (31, 31, r"""#include "FireteamRoundHud.h"
"""),
    (93, 93, r"""#include "FireteamCombatRules.h"
"""),
    (464, 464, r"""static bool s_bReloading = false;
static float s_fReloadRemaining = 0;
void FT_WeaponHudSetReload(bool reloading, float remaining)
{
    s_bReloading = reloading;
    s_fReloadRemaining = FTCombatRules::Clamp(remaining, 0, 30, 0);
}
"""),
    (11343, 11385, r"""                (float)nScreenW - s_pWeaponName->GetWidth() - 28.0f,
"""),
    (14467, 14492, r"""        char szAmmo[80];
"""),
    (14493, 14681, r"""        const bool bBottomless = FT_RoundHudIsBottomlessActive();
        if(bBottomless)
            sprintf(szAmmo, "BOTTOMLESS  %u/%u", (unsigned)s_nPrimaryClip, (unsigned)s_nPrimaryReserve);
        else if(s_bReloading)
            sprintf(szAmmo, "RELOADING %.1fs  %u/%u", s_fReloadRemaining,
                (unsigned)s_nPrimaryClip, (unsigned)s_nPrimaryReserve);
        else if(s_nPrimaryClip == 0 && s_nPrimaryReserve == 0)
            sprintf(szAmmo, "EMPTY  0/0");
        else if(s_nPrimaryClip == 0)
            sprintf(szAmmo, "R: RELOAD  0/%u", (unsigned)s_nPrimaryReserve);
        else sprintf(szAmmo, "%u/%u", (unsigned)s_nPrimaryClip, (unsigned)s_nPrimaryReserve);
"""),
    (14719, 14719, r"""        s_pAmmoText->SetColor(s_nPrimaryClip == 0 && !bBottomless ? 0xFFFFC070 : 0xFFFFFFFF);
"""),
    (14753, 14791, r"""            (float)nScreenW - s_pAmmoText->GetWidth() - 28.0f,
"""),
])
edit('src/cshell/FireteamWeaponHud.h', 'fedb849524fe34e5d50212a441ee6c4732f8811d20f8c9ca742534569b3941d5', [
    (140, 140, r"""void FT_WeaponHudSetReload(bool bReloading, float fRemaining);
"""),
])
edit('src/cshell/ltclientshell.cpp', '0debf5589bbb5054e50443563ad4a5185f1ed2b70d33c93bb5c0a98a5384df12', [
    (10024, 10024, r"""    FT_HealthHudInit();
"""),
    (18148, 18148, r"""    FT_HealthHudReset();
    FT_RadarHudReset();
"""),
    (21898, 21898, r"""        }
        break;
    case MSG_SC_DAMAGE_DIRECTION:
        {
            const LTVector source = pMessage->ReadLTVector();
            const uint8 damage = pMessage->Readuint8();
            FT_HealthHudDamage(source, damage);
        }
        break;
    case MSG_SC_INTERMISSION:
        {
            const uint16 round = pMessage->Readuint16();
            const float remaining = pMessage->Readfloat();
            FT_RoundHudSetIntermission(round, remaining);
"""),
    (34312, 34356, r""""""),
    (34404, 34523, r""""""),
    (35330, 35330, r"""        // Scope masks belong underneath essential survival information.
        // Otherwise the optic can hide health, damage direction and rounds.
        if(!bFreecam) FT_RenderHealthHud(m_hCamera);
        FT_RenderRoundHud();
        if(!bFreecam && !FT_SettingsIsOpen()) FT_RenderRadarHud();
"""),
    (81963, 81963, r"""    FT_HealthHudTerm();
"""),
])
edit('src/cshell/playerclnt.cpp', 'ccc55192d686e96b43c82ef3f6fa87e6c27c1661089b40e344b3bdf5a3152d90', [
    (351, 351, r"""#include "FireteamCombatRules.h"
"""),
    (18445, 18445, r"""
    FT_WeaponHudSetReload(m_bReloading,
        m_bReloading ? m_fReloadComplete - g_pLTClient->GetTime() : 0);
"""),
    (41117, 41211, r"""        FTCombatRules::ReloadSeconds(pDef->fReloadSeconds);
"""),
])
new('src/shared/FireteamCombatRules.h', r"""#ifndef __FIRETEAM_COMBAT_RULES_H__
#define __FIRETEAM_COMBAT_RULES_H__
#include <float.h>

// Engine-independent rules shared by the server and local presentation.
namespace FTCombatRules
{
    inline bool Finite(float x) { return x >= -FLT_MAX && x <= FLT_MAX; }
    inline float Clamp(float x, float lo, float hi, float fallback)
    {
        if(!Finite(x)) return fallback;
        return x < lo ? lo : (x > hi ? hi : x);
    }
    inline float ReloadSeconds(float authored)
    {
        // Existing invalid/empty legacy definitions keep the short fallback.
        // Real definitions are authoritative, not the old universal 125ms.
        return Clamp(authored, 0.125f, 30.0f, 0.125f);
    }
    inline unsigned PendingSpawns(unsigned target, unsigned killed, unsigned alive)
    {
        if(killed >= target) return 0;
        const unsigned remaining = target - killed;
        return alive < remaining ? remaining - alive : 0;
    }
}
#endif
""")
new('src/shared/FireteamFeedbackQueue.h', r"""#ifndef __FIRETEAM_FEEDBACK_QUEUE_H__
#define __FIRETEAM_FEEDBACK_QUEUE_H__
#include "FireteamCombatRules.h"
namespace FTFeedbackRules
{
    class RecentQueue
    {
    public:
        RecentQueue() : count(0) {}
        void Clear() { count=0; }
        void Push(unsigned char kind, float now)
        {
            if(!kind || !FTCombatRules::Finite(now)) return;
            for(unsigned i=0;i<count;++i) if(kinds[i]==kind)
            { times[i]=now; return; }
            if(count==4) EraseFirst();
            kinds[count]=kind; times[count]=now; ++count;
        }
        unsigned char Pop(float now)
        {
            if(!FTCombatRules::Finite(now)) { Clear(); return 0; }
            while(count)
            {
                const unsigned char kind=kinds[0];
                const float age=now-times[0];
                EraseFirst();
                if(age>=0 && age<=2.5f) return kind;
            }
            return 0;
        }
    private:
        void EraseFirst()
        {
            for(unsigned i=1;i<count;++i) { kinds[i-1]=kinds[i]; times[i-1]=times[i]; }
            if(count) --count;
        }
        unsigned char kinds[4]; float times[4]; unsigned count;
    };
}
#endif
""")
edit('src/shared/FireteamInfectedDefs.h', '130b7e071522437822e4cb2aa736aa6b40293ba4b0b3265f76e73afc05d8c271', [
    (469, 469, r"""    float fAttackRecoverySeconds;
"""),
    (4106, 4106, r"""    else if(_stricmp(pKey, "attack_recovery_seconds") == 0)
        def.fAttackRecoverySeconds = (float)atof(pValue);
"""),
])
new('src/shared/FireteamPresentationRules.h', r"""#ifndef __FIRETEAM_PRESENTATION_RULES_H__
#define __FIRETEAM_PRESENTATION_RULES_H__
#include "FireteamCombatRules.h"
#include <math.h>
namespace FTPresentationRules
{
    inline float RadarRadius(float screenHeight, float scale)
    {
        return FTCombatRules::Clamp(screenHeight * 0.09f, 54.0f, 84.0f, 54.0f) *
               FTCombatRules::Clamp(scale, 0.75f, 1.75f, 1.0f);
    }
    // 0 front, 1 right, 2 back, 3 left, -1 unspecified. Dot products use
    // a horizontal camera basis; this is presentation, never damage authority.
    inline int DamageSide(float right, float forward)
    {
        if(!FTCombatRules::Finite(right) || !FTCombatRules::Finite(forward) ||
           (fabsf(right) < 0.001f && fabsf(forward) < 0.001f)) return -1;
        if(fabsf(right) > fabsf(forward)) return right >= 0 ? 1 : 3;
        return forward >= 0 ? 0 : 2;
    }
    inline unsigned Countdown(float deadline, float now)
    {
        if(!FTCombatRules::Finite(deadline) || !FTCombatRules::Finite(now) || deadline <= now)
            return 0;
        return (unsigned)ceilf(FTCombatRules::Clamp(deadline - now, 0, 120, 0));
    }
}
#endif
""")
edit('src/shared/msgids.h', '6f28acd58c9197c5c02c724675c03ce16ff683310c1d20b4931e5e3bed308beb', [
    (2582, 2582, r"""        MSG_SC_DAMAGE_DIRECTION,   // source position + actual damage, victim only
        MSG_SC_INTERMISSION,       // round + server time remaining; no round authority
"""),
])
