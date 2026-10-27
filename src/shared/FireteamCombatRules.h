#ifndef __FIRETEAM_COMBAT_RULES_H__
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
