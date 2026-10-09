#ifndef __FIRETEAM_HIT_MATH_H__
#define __FIRETEAM_HIT_MATH_H__

// Portable, server-authoritative narrow-phase geometry. This deliberately
// has NO LithTech dependency so its full 3D bullet math can be unit tested.
#include <math.h>

namespace FTHitMath
{
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
        const float ox = (from.x - center.x) / radii.x;
        const float oy = (from.y - center.y) / radii.y;
        const float oz = (from.z - center.z) / radii.z;
        const float dx = direction.x / radii.x;
        const float dy = direction.y / radii.y;
        const float dz = direction.z / radii.z;

        const float a = dx * dx + dy * dy + dz * dz;
        const float halfB = ox * dx + oy * dy + oz * dz;
        const float c = ox * ox + oy * oy + oz * oz - 1.0f;
        if(!(a > 0.00000001f))
            return false;
        const float discriminant = halfB * halfB - a * c;
        if(!(discriminant >= 0.0f))
            return false;

        const float root = sqrtf(discriminant);
        // Beginning inside the body is a legitimate short-range collision.
        const float t = c <= 0.0f ? 0.0f :
            (-halfB - root >= 0.0f
                ? (-halfB - root) / a : (-halfB + root) / a);
        if(!(t >= 0.0f && t <= maxDistance))
            return false;
        outDistance = t;
        return true;
    }
}

#endif
