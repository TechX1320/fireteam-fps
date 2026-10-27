#include "../src/shared/FireteamAnimationRules.h"
#include <assert.h>
#include <stdio.h>
int main()
{
    using FTAnimationRules::IsAttackNamedCandidate;
    using FTAnimationRules::ShouldSkipAnimation;
    assert(IsAttackNamedCandidate("zombie_attack_1"));
    assert(IsAttackNamedCandidate("MELEE_LEFT"));
    assert(IsAttackNamedCandidate("V_ATK_F"));
    assert(!IsAttackNamedCandidate("VLST"));
    assert(!IsAttackNamedCandidate("VDIE"));
    assert(!IsAttackNamedCandidate("attack/../../bad"));
    assert(!IsAttackNamedCandidate(""));
    assert(ShouldSkipAnimation("VLST", "VLST", true));
    assert(!ShouldSkipAnimation("UMFi", "UMFi", false));
    assert(!ShouldSkipAnimation("UMFi", "VLST", true));
    assert(ShouldSkipAnimation("VLST", "", false));
    puts("PASS: model attack-name filters and repeatable one-shot animations");
    return 0;
}
