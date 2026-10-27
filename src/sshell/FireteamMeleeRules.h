#ifndef __FIRETEAM_MELEE_RULES_H__
#define __FIRETEAM_MELEE_RULES_H__
#include "../shared/FireteamCombatRules.h"
#include <math.h>

namespace FTMeleeRules
{
    inline float WindupSeconds(float configured, float cooldown)
    {
        if(!FTCombatRules::Finite(configured) || !FTCombatRules::Finite(cooldown) ||
           configured <= 0.0f || cooldown <= 0.0f) return 0.0f;
        const float cap = cooldown * 0.75f < 2.0f ? cooldown * 0.75f : 2.0f;
        return configured < cap ? configured : cap;
    }
    inline float RecoverySeconds(float configured, float cooldown, float windup)
    {
        if(!FTCombatRules::Finite(configured) || !FTCombatRules::Finite(cooldown) ||
           !FTCombatRules::Finite(windup) || configured <= 0.0f) return 0.0f;
        const float left = cooldown - windup;
        if(left <= 0.0f) return 0.0f;
        const float cap = left < 2.0f ? left : 2.0f;
        return configured < cap ? configured : cap;
    }
    inline bool CanConnect(float x, float y, float z, float range, float height)
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
}
#endif
