#ifndef NX3D_SHARED_BOMB_DAMAGE_HH
#define NX3D_SHARED_BOMB_DAMAGE_HH

#include <shared/nx_objects.hh>

#include <algorithm>
#include <cmath>

namespace nx3d
{
inline constexpr float kBombBlastRadius = findNXObject("bomb")->effect.radius;
inline constexpr float kBombMaxDamage = findNXObject("bomb")->effect.maxDamage;

inline float bombBlastDamage(float distance)
{
    if (!std::isfinite(distance) || distance >= kBombBlastRadius)
        return 0.0f;
    return kBombMaxDamage * (1.0f - std::max(0.0f, distance) / kBombBlastRadius);
}
} // namespace nx3d

#endif
