#ifndef __FIRETEAM_HIT_MATH_H__
#define __FIRETEAM_HIT_MATH_H__

// Portable, server-authoritative narrow-phase geometry. This deliberately
// has NO LithTech dependency so its full 3D bullet math can be unit tested.
#include <math.h>

namespace FTHitMath
{
    // Model animation user dimensions are posture-specific. Never let a
    // crouch/idle frame permanently shrink the authoritative bullet broad
    // phase to a tiny target. Body caps come from the server, not the client.
    inline bool SafeModelHalfExtent(float authored, float sampled)
    {
        return authored > 0.0f && sampled > 0.0f &&
            sampled >= authored * 0.90f && sampled < authored;
    }
    inline float TorsoRadius(float collisionHalfExtent)
    {
        // Chest/shoulder shell stays inside the server-clamped physical box.
        return collisionHalfExtent * 0.78f;
    }
    struct Vec3
    {
        float x, y, z;
        Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    };

    inline bool RayEllipsoid(const Vec3 &from, const Vec3 &direction,
        const Vec3 &center, const Vec3 &radii, float maxDistance,
        float &outDistance)
    {
        if(!(radii.x > 0.0f && radii.y > 0.0f && radii.z > 0.0f) ||
           !(maxDistance > 0.0f))
            return false;
        // Float discriminants suffer catastrophic cancellation once the
        // target is thousands of units away: an on-silhouette shot can be
        // reported as a miss (or an off-target shot as a hit). Work in
        // double precision for both local coordinates and quadratic math.
        // The input ray remains the server-validated world ray.
        const double ox = ((double)from.x - center.x) / radii.x;
        const double oy = ((double)from.y - center.y) / radii.y;
        const double oz = ((double)from.z - center.z) / radii.z;
        const double dx = (double)direction.x / radii.x;
        const double dy = (double)direction.y / radii.y;
        const double dz = (double)direction.z / radii.z;

        const double a = dx * dx + dy * dy + dz * dz;
        const double halfB = ox * dx + oy * dy + oz * dz;
        const double c = ox * ox + oy * oy + oz * oz - 1.0;
        if(!(a > 0.00000001))
            return false;
        const double discriminant = halfB * halfB - a * c;
        if(!(discriminant >= 0.0))
            return false;

        const double root = sqrt(discriminant);
        // Beginning inside the body is a legitimate short-range collision.
        const double t = c <= 0.0 ? 0.0 :
            (-halfB - root >= 0.0
                ? (-halfB - root) / a : (-halfB + root) / a);
        if(!(t >= 0.0 && t <= (double)maxDistance))
            return false;
        outDistance = (float)t;
        return true;
    }
}

#endif
