#ifndef __FIRETEAM_ANIMATION_RULES_H__
#define __FIRETEAM_ANIMATION_RULES_H__
#include <ctype.h>
#include <string.h>

// Portable safeguards. An animation-name string taken from an imported LTB
// is never itself evidence that the live composed model supports that clip;
// the server still validates every candidate with GetAnimIndex.
namespace FTAnimationRules
{
    inline bool IsAttackNamedCandidate(const char *text)
    {
        if(!text || !*text) return false;
        char upper[64];
        unsigned i = 0;
        for(; text[i] && i < sizeof(upper) - 1; ++i)
        {
            const unsigned char c = (unsigned char)text[i];
            if(!isalnum(c) && c != '_') return false;
            upper[i] = (char)toupper(c);
        }
        if(text[i]) return false;
        upper[i] = '\0';
        return strstr(upper, "ATTACK") != 0 || strstr(upper, "ATK") != 0 ||
            strstr(upper, "MELEE") != 0;
    }
    inline bool ShouldSkipAnimation(const char *current, const char *next,
        bool looping)
    {
        if(!next || !*next) return true;
        // Repeated one-shots MUST replay even when their name is unchanged.
        return looping && current && strcmp(current, next) == 0;
    }
}
#endif
