#ifndef __FIRETEAM_MELEE_RULES_H__
#define __FIRETEAM_MELEE_RULES_H__

#include <math.h>

// Portable rules for resolving a server-authoritative melee swing. A hit
// requires proximity at impact, not merely at animation start.
namespace FTMeleeRules
{
    inline float WindupSeconds(float configuredSeconds, float cooldownSeconds)
    {
        if(!(configuredSeconds > 0.0f) || !(cooldownSeconds > 0.0f))
            return 0.0f;
        // Never let a long modded windup outlast the attack's cooldown.
        const float maxWindup = cooldownSeconds * 0.75f;
        return configuredSeconds < maxWindup ? configuredSeconds : maxWindup;
    }

    inline bool CanConnect(float deltaX, float deltaY, float deltaZ,
        float horizontalRange, float verticalReach)
    {
        if(!(horizontalRange > 0.0f) || !(verticalReach > 0.0f))
            return false;
        return deltaX * deltaX + deltaZ * deltaZ <=
                   horizontalRange * horizontalRange &&
               fabsf(deltaY) <= verticalReach;
    }
}

#endif
