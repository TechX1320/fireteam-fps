#ifndef __FIRETEAM_PRESENTATION_RULES_H__
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
