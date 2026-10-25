#ifndef NX3D_SHARED_FALL_DEATH_HH
#define NX3D_SHARED_FALL_DEATH_HH

namespace nx3d
{
// Generated Dust II's lowest floor is Y=-112. This plane leaves clearance beneath
// the map while ending an out-of-bounds fall promptly.
inline constexpr float kFallDeathY = -256.0f;
}

#endif
