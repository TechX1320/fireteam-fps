// Portable smoke test: g++ -std=c++11 -Wall -Wextra
// tests/test-hit-math.cpp -o hit-math-test && ./hit-math-test
// No LithTech SDK needed for these pure vector equations.
#include "../src/sshell/FireteamHitMath.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

using FTHitMath::Vec3;
using FTHitMath::RayEllipsoid;

static bool Hits(const Vec3& origin, const Vec3& direction,
                 const Vec3& center, const Vec3& radii,
                 float range = 200.0f)
{
    float t = -1.0f;
    return RayEllipsoid(origin, direction, center, radii, range, t);
}

int main()
{
    float distance = -1.0f;
    const Vec3 center(0, 0, 0);
    const Vec3 torso(15, 30, 15);
    const Vec3 forward(0, 0, 1);

    // Real shots hit the volume at its entry, not its oversized broad box.
    assert(RayEllipsoid(Vec3(0, 0, -100), forward, center,
                        torso, 200, distance));
    assert(fabsf(distance - 85.0f) < 0.01f);
    // One foot above a fake animation box or on another floor must NOT hit.
    assert(!Hits(Vec3(0, 62, -100), forward, center, torso));
    assert(!Hits(Vec3(0, 160, -100), forward, center, torso));
    // Side-on firing outside the tightened body misses entirely.
    assert(!Hits(Vec3(24, 0, -100), forward, center, torso));
    // Exact 3D ray can legitimately hit from elevated floor looking down.
    const float inv = 1.0f / sqrtf(2.0f);
    assert(Hits(Vec3(0, 70, -70), Vec3(0, -inv, inv),
                center, torso));
    // Head-only accuracy: a shot ABOVE a small head cannot award headshot.
    assert(Hits(Vec3(0, 40, -100), forward, Vec3(0, 40, 0),
                Vec3(10, 9, 10)));
    assert(!Hits(Vec3(0, 55, -100), forward, Vec3(0, 40, 0),
                 Vec3(10, 9, 10)));
    // Bounds and degenerate vectors must never produce false hits.
    assert(!Hits(Vec3(0, 0, -100), forward, center, torso, 75));
    assert(!Hits(Vec3(0, 0, -100), Vec3(0, 0, 0), center, torso));
    assert(!Hits(Vec3(0, 0, -100), forward, center, Vec3(0, 30, 15)));
    puts("PASS: 3D ray/ellipsoid geometry, ranges, floors, head and misses");
    return 0;
}
