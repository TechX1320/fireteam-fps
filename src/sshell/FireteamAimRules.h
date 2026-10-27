#ifndef __FIRETEAM_AIM_RULES_H__
#define __FIRETEAM_AIM_RULES_H__

// No LithTech dependencies: the server's accuracy selection can be tested
// independently of weapon rendering, importing and networking.
namespace FTAimRules
{
    inline unsigned int SelectPerturb(unsigned int minPerturb,
        unsigned int maxPerturb, bool hasScope, bool zoomed,
        float movement01)
    {
        // Preserve the existing no-spread behavior of invalid old configs.
        if(maxPerturb < minPerturb)
            return 0;

        if(hasScope)
        {
            // Curated legacy scoped weapons may have no CA perturb metadata.
            // Hip-fire must not silently become a perfect center ray.
            if(minPerturb == 0 && maxPerturb == 0)
                return zoomed ? 0u : 90u;
            return zoomed ? minPerturb : maxPerturb;
        }

        // A stationary shot gets the authored minimum; moving shots ramp to
        // the authored maximum. Reject NaN and clamp unusual velocities.
        if(!(movement01 >= 0.0f)) movement01 = 0.0f;
        if(movement01 > 1.0f) movement01 = 1.0f;
        return minPerturb + (unsigned int)(
            (float)(maxPerturb - minPerturb) * movement01 + 0.5f);
    }
}

#endif
