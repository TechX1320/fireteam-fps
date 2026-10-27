edit('src/sshell/FireteamMeleeRules.h', 'ec8cebf7bfe0835e25396a723821f7ab7b59b61c11b845415d86a078a7d80153', [
    (70, 71, r"""#include "../shared/FireteamCombatRules.h"
"""),
    (90, 228, r""""""),
    (253, 332, r"""    inline float WindupSeconds(float configured, float cooldown)
"""),
    (338, 640, r"""        if(!FTCombatRules::Finite(configured) || !FTCombatRules::Finite(cooldown) ||
           configured <= 0.0f || cooldown <= 0.0f) return 0.0f;
        const float cap = cooldown * 0.75f < 2.0f ? cooldown * 0.75f : 2.0f;
        return configured < cap ? configured : cap;
"""),
    (646, 768, r"""    inline float RecoverySeconds(float configured, float cooldown, float windup)
"""),
    (774, 1020, r"""        if(!FTCombatRules::Finite(configured) || !FTCombatRules::Finite(cooldown) ||
           !FTCombatRules::Finite(windup) || configured <= 0.0f) return 0.0f;
        const float left = cooldown - windup;
        if(left <= 0.0f) return 0.0f;
        const float cap = left < 2.0f ? left : 2.0f;
        return configured < cap ? configured : cap;
"""),
    (1026, 1026, r"""    inline bool CanConnect(float x, float y, float z, float range, float height)
    {
        if(!FTCombatRules::Finite(x) || !FTCombatRules::Finite(y) ||
           !FTCombatRules::Finite(z) || !FTCombatRules::Finite(range) ||
           !FTCombatRules::Finite(height) || range <= 0 || height <= 0) return false;
        // Ratios avoid overflow for finite but unusually large coordinates.
        const double dx = (double)x / range, dz = (double)z / range;
        return dx * dx + dz * dz <= 1.0 && fabsf(y) <= height;
    }
    inline bool KeepTarget(float currentDistanceSquared, float bestDistanceSquared)
    {
        // Don't oscillate between almost equidistant squad members.
        return FTCombatRules::Finite(currentDistanceSquared) &&
            FTCombatRules::Finite(bestDistanceSquared) && currentDistanceSquared >= 0 && bestDistanceSquared >= 0 &&
            currentDistanceSquared <= bestDistanceSquared * 1.5625f;
    }
    inline bool SameLife(unsigned expectedInstance, unsigned expectedDeaths,
        unsigned actualInstance, unsigned actualDeaths, bool targetable)
    {
        return targetable && expectedInstance != 0 && expectedInstance == actualInstance &&
            expectedDeaths == actualDeaths;
    }
    class StrikeClock
    {
    public:
        StrikeClock() : m_contact(0), m_unlock(0), m_pending(false) {}
        void Begin(float now, float windup, float recovery)
        {
            if(!FTCombatRules::Finite(now) || !FTCombatRules::Finite(windup) ||
               !FTCombatRules::Finite(recovery) || windup < 0 || recovery < 0)
            { Cancel(); return; }
            m_contact = now + windup;
            m_unlock = m_contact + recovery;
            m_pending = true;
        }
        bool TakeImpact(float now)
        {
            if(!FTCombatRules::Finite(now) || !m_pending || now < m_contact) return false;
            m_pending = false; // Exactly once, including hitches and misses.
            return true;
        }
        bool Locked(float now) const { return m_pending || now < m_unlock; }
        void Cancel() { m_pending = false; m_contact = m_unlock = 0; }
    private:
        float m_contact, m_unlock;
        bool m_pending;
    };
"""),
    (1028, 1029, r""""""),
])
edit('src/sshell/FireteamSpawner.cpp', '8a14a742fc5c8988675d96883fa2c804823de13b6398b1a1ff59781b0f0e680d', [
    (1876, 1876, r"""static float s_fRoundStartedAt = 0.0f;
"""),
    (11054, 11054, r"""    // A separate appended message preserves all existing packet layouts.
    if(nState == 0 && !hClient) return;
    ILTMessage_Write *pBreak = LTNULL;
    if(g_pLTSCommon->CreateMessage(pBreak) == LT_OK && pBreak)
    {
        const float remaining = s_bRoundIntermission
            ? s_fNextRoundTime - g_pLTServer->GetTime() : 0.0f;
        pBreak->IncRef();
        pBreak->Writeuint8(MSG_SC_INTERMISSION);
        pBreak->Writeuint16((uint16)s_nRound);
        pBreak->Writefloat(remaining > 0.0f ? remaining : 0.0f);
        g_pLTServer->SendToClient(pBreak->Read(), hClient, MESSAGE_GUARANTEED);
        pBreak->DecRef();
    }
"""),
    (33389, 33389, r"""    s_fRoundStartedAt = g_pLTServer->GetTime();
"""),
    (49052, 49052, r"""            g_pLTServer->CPrint(
                "Fireteam pacing: round=%u seconds=%.2f target=%u peak_limit=%u bosses=%u restores=%u.",
                s_nRound, g_pLTServer->GetTime() - s_fRoundStartedAt,
                s_nRoundTarget, s_nMaxAlive, s_nTankersSpawned, s_nMissingInfectedRestores);
"""),
])
edit('src/sshell/FireteamZombie.cpp', '657e58d36ac07af8e4502066cd19afde2e552ca04707d232039c241ff378e651', [
    (1206, 1206, r"""    s_bDifficultyLoaded = false; // The next map may use another difficulty.
"""),
    (2566, 2602, r""""""),
    (2638, 2638, r"""    m_nAttackTargetInstance(0),
    m_nAttackTargetDeaths(0),
    m_hChaseTarget(LTNULL),
    m_nChaseTargetInstance(0),
"""),
    (33184, 33184, r"""    uint32 nBestInstance = 0;
"""),
    (33215, 33215, r"""    float fCurrentDist = FLT_MAX;
    bool bCurrentValid = false;
"""),
    (33831, 33831, r"""        if(hObj == m_hChaseTarget &&
           pPlayer->GetCombatInstance() == m_nChaseTargetInstance)
        {
            bCurrentValid = true;
            fCurrentDist = fDist;
        }
"""),
    (33928, 33928, r"""            nBestInstance = pPlayer->GetCombatInstance();
"""),
    (33944, 33945, r"""    if(bCurrentValid && FTMeleeRules::KeepTarget(fCurrentDist, fBestDist))
        return m_hChaseTarget;
    m_hChaseTarget = hBest;
    m_nChaseTargetInstance = nBestInstance;
"""),
    (39217, 39274, r"""void FireteamZombie::ResolveMeleeStrike(
    HOBJECT hTarget, uint32 nInstance, uint32 nDeaths)
"""),
    (39276, 39388, r"""    if(!hTarget || m_bDying || m_nHealth == 0) return;
    // Do not dereference a cached HOBJECT after a disconnect. Match it against
    // live objects first, then verify generation and life. A recycled handle
    // must not transfer an old swing onto a new player or a respawned life.
    HOBJECT hLive = LTNULL;
    for(HOBJECT h = g_pLTServer->GetNextObject(LTNULL); h;
        h = g_pLTServer->GetNextObject(h))
        if(h == hTarget) { hLive = h; break; }
    if(!hLive) return;
"""),
    (39452, 39516, r"""    HCLASS hTargetClass = g_pLTServer->GetObjectClass(hLive);
"""),
    (39557, 39782, r"""       !g_pLTServer->IsKindOf(hTargetClass, hPlayerClass)) return;
    CPlayerSrvr *pPlayer = (CPlayerSrvr*)g_pLTServer->HandleToObject(hLive);
    if(!pPlayer || !FTMeleeRules::SameLife(nInstance, nDeaths,
       pPlayer->GetCombatInstance(), pPlayer->GetMatchDeaths(), pPlayer->IsTargetable())) return;
"""),
    (40329, 40376, r"""    pPlayer->ApplyDamage(m_Def.nAttackDamage, &vZombie);
"""),
    (41469, 41652, r"""    const float fAttackNow = g_pLTServer->GetTime();
    if(m_StrikeClock.TakeImpact(fAttackNow))
"""),
    (41658, 42037, r"""        ResolveMeleeStrike(m_hPendingAttackTarget,
            m_nAttackTargetInstance, m_nAttackTargetDeaths);
"""),
    (42078, 42162, r"""    }
    if(m_StrikeClock.Locked(fAttackNow))
    {
        // A committed swing cannot chase/turn with a dodging player. Recovery
        // also applies to a missed swing, making a heavy attack punishable.
        LTVector vStop(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vStop);
        m_nMotionStepsRemaining = 0;
        m_eBehaviorState = kBehaviorAttack;
        return;
"""),
    (44773, 44815, r"""    if(FTMeleeRules::CanConnect(vTarget.x - vPos.x, vTarget.y - vPos.y,
           vTarget.z - vPos.z, kAttackRange, m_vCollisionDims.y + 18.0f) &&
"""),
    (46286, 46353, r"""                if(pCandidate && pCandidate[0] &&
                   g_pLTServer->GetAnimIndex(m_hObject, (char*)pCandidate) != INVALID_MODEL_ANIM)
"""),
    (47052, 47205, r""""""),
    (47336, 47367, r"""            const float fRecovery = FTMeleeRules::RecoverySeconds(
                m_Def.fAttackRecoverySeconds, m_Def.fAttackCooldown, fWindup);
            CPlayerSrvr *pVictim = (CPlayerSrvr*)g_pLTServer->HandleToObject(hTarget);
            if(!pVictim || !pVictim->IsTargetable())
"""),
    (47381, 47483, r"""                m_hPendingAttackTarget = LTNULL;
                m_StrikeClock.Cancel();
                m_nMotionStepsRemaining = 0;
                return;
"""),
    (47497, 47514, r"""            m_hPendingAttackTarget = hTarget;
            m_nAttackTargetInstance = pVictim->GetCombatInstance();
            m_nAttackTargetDeaths = pVictim->GetMatchDeaths();
            m_StrikeClock.Begin(fAttackNow, fWindup, fRecovery);
            if(m_StrikeClock.TakeImpact(fAttackNow))
"""),
    (47528, 47709, r"""                ResolveMeleeStrike(hTarget, m_nAttackTargetInstance, m_nAttackTargetDeaths);
                m_hPendingAttackTarget = LTNULL;
"""),
    (48044, 48092, r"""                m_fAttackVoiceCooldown = FTCombatRules::Clamp(
                    m_Def.fAttackCooldown, 0.25f, 1.10f, 1.10f);
"""),
    (87150, 87195, r"""            m_StrikeClock.Cancel();
"""),
])
edit('src/sshell/FireteamZombie.h', 'ee04d125c8e062dc672d702d91f1d45533c25e080fb935dd202639a05144c7e7', [
    (124, 124, r"""#include "FireteamMeleeRules.h"
"""),
    (1267, 1313, r"""    void ResolveMeleeStrike(HOBJECT hTarget, uint32 nInstance, uint32 nDeaths);
"""),
    (2232, 2268, r"""    FTMeleeRules::StrikeClock m_StrikeClock;
"""),
    (2304, 2304, r"""    uint32 m_nAttackTargetInstance;
    uint32 m_nAttackTargetDeaths;
    HOBJECT m_hChaseTarget;
    uint32 m_nChaseTargetInstance;
"""),
])
edit('src/sshell/playersrvr.cpp', '59e4b327e0594658524b4fab0df8f68a7e99b8d3bfe1db31bf58a692f500b83c', [
    (993, 993, r"""#include "FireteamCombatRules.h"
"""),
    (43387, 43432, r"""void CPlayerSrvr::ApplyDamage(uint8 nDamage, const LTVector *pSource)
"""),
    (44226, 44226, r"""    if(m_hClient && pSource && FTCombatRules::Finite(pSource->x) &&
       FTCombatRules::Finite(pSource->y) && FTCombatRules::Finite(pSource->z))
    {
        ILTMessage_Write *pCue = LTNULL;
        if(g_pLTSCommon->CreateMessage(pCue) == LT_OK && pCue)
        {
            pCue->IncRef();
            pCue->Writeuint8(MSG_SC_DAMAGE_DIRECTION);
            pCue->WriteLTVector(*pSource);
            pCue->Writeuint8(nActuallyTaken);
            g_pLTServer->SendToClient(pCue->Read(), m_hClient, MESSAGE_GUARANTEED);
            pCue->DecRef();
        }
    }
"""),
    (58170, 58208, r"""    const float fAimLengthSquared = vDir.MagSqr();
    if(!FTCombatRules::Finite(fAimLengthSquared) || fAimLengthSquared < 0.0001f)
"""),
    (58224, 58230, r""""""),
    (75441, 76022, r"""    const float fReloadSeconds = FTCombatRules::ReloadSeconds(pDef->fReloadSeconds);
    m_fReloadComplete = g_pLTServer->GetTime() + fReloadSeconds;
"""),
    (76047, 76236, r"""        "Fireteam weapon: %s reload accepted; authoritative duration %.3fs.",
        pDef->sName, fReloadSeconds);
"""),
])
edit('src/sshell/playersrvr.h', '42c4b14a2a7575a7f82cff6a82d40d0789047e59207c088c12bfe850090965b2', [
    (2840, 2840, r"""        m_nCombatInstance = NextCombatInstance();
"""),
    (5131, 5131, r"""    uint32              GetCombatInstance() const { return m_nCombatInstance; }
"""),
    (6784, 6836, r"""    void                ApplyDamage(uint8 nDamage, const LTVector *pSource = LTNULL);
"""),
    (7848, 7848, r"""    static uint32 NextCombatInstance()
    {
        static uint32 serial = 0;
        if(++serial == 0) ++serial;
        return serial;
    }
    uint32 m_nCombatInstance;
"""),
])
new('test-gameplay.cmd', r"""@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo Use a Visual Studio 2022 Developer Command Prompt with CMake installed.
  exit /b 1
)
cmake -S tests -B out\portable-win32 -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 exit /b 1
cmake --build out\portable-win32 --config Debug
if errorlevel 1 exit /b 1
ctest --test-dir out\portable-win32 -C Debug --output-on-failure
if errorlevel 1 exit /b 1
where dotnet >nul 2>nul
if errorlevel 1 (
  echo [SKIP] .NET settings tests: dotnet not installed.
  exit /b 0
)
dotnet run --project tests\LauncherSettings.Tests\LauncherSettings.Tests.csproj -c Release
exit /b %errorlevel%
""")
edit('tests/CMakeLists.txt', '6761560d8a1564941fffb361a16702c0081234a32a55e816800ed4e77f517620', [
    (686, 686, r"""
find_program(FT_POWERSHELL NAMES pwsh powershell)
if(FT_POWERSHELL)
    add_test(NAME config-preservation COMMAND "${FT_POWERSHELL}" -NoProfile
        -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/test-config-preservation.ps1")
endif()
"""),
])
