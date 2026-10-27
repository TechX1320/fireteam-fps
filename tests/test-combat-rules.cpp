// Portable gameplay contract: g++ -std=c++11 -Wall -Wextra -Werror
// tests/test-combat-rules.cpp -o combat-rules-test && ./combat-rules-test
#include "../src/sshell/FireteamAimRules.h"
#include "../src/sshell/FireteamMeleeRules.h"
#include <assert.h>
#include <math.h>
#include <limits>
#include <stdio.h>

int main()
{
    using FTAimRules::SelectPerturb;
    // Legacy imported scoped rifles: zoom works, hip-fire is not laser-accurate.
    assert(SelectPerturb(0, 0, true, false, 0) == 90u);
    assert(SelectPerturb(0, 0, true, true, 0) == 0u);
    // Real CA perturb values must override the legacy fallback.
    assert(SelectPerturb(4, 48, true, false, 0) == 48u);
    assert(SelectPerturb(4, 48, true, true, 1) == 4u);
    // Unscoped guns preserve definition-driven movement interpolation.
    assert(SelectPerturb(12, 52, false, false, 0) == 12u);
    assert(SelectPerturb(12, 52, false, false, 0.5f) == 32u);
    assert(SelectPerturb(12, 52, false, false, 1) == 52u);
    assert(SelectPerturb(12, 52, false, false, 3) == 52u);
    assert(SelectPerturb(12, 52, false, false, -1) == 12u);
    assert(SelectPerturb(12, 52, false, false,
        std::numeric_limits<float>::quiet_NaN()) == 12u);
    assert(SelectPerturb(52, 12, false, false, 0) == 0u);

    using FTMeleeRules::WindupSeconds;
    using FTMeleeRules::CanConnect;
    assert(fabsf(WindupSeconds(0.28f, 1.0f) - 0.28f) < 0.0001f);
    assert(fabsf(WindupSeconds(1.0f, 0.6f) - 0.45f) < 0.0001f);
    assert(WindupSeconds(0.0f, 1.0f) == 0.0f);
    assert(WindupSeconds(-1.0f, 1.0f) == 0.0f);
    // Same-floor attack connects only when the victim remains in reach.
    assert(CanConnect(55, 0, 0, 55, 71));
    assert(!CanConnect(55.1f, 0, 0, 55, 71));
    assert(!CanConnect(0, 110, 0, 55, 71));
    assert(!CanConnect(0, 0, 0, 0, 71));
    assert(!CanConnect(std::numeric_limits<float>::quiet_NaN(), 0, 0, 55, 71));
    puts("PASS: scoped/hip spread, movement spread and dodgeable melee rules");
    return 0;
}
