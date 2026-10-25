#ifndef NX3D_SHARED_BOMB_SITES_HH
#define NX3D_SHARED_BOMB_SITES_HH

#include <array>
#include <cmath>

namespace nx3d::bomb_sites
{
struct Site
{
    const char* name;
    float minX, maxX;
    float minY, maxY;
    float minZ, maxZ;
};

// Generated Dust II preserves the A platform (Y=96) and B courtyard (Y=0).
// Bounds use the player's eye position, 64 units above the ground.
inline constexpr std::array<Site, 2> kSites{{
        {"A", 920.0f, 1490.0f, 130.0f, 260.0f, -2710.0f, -2110.0f},
        {"B", -1910.0f, -1290.0f, 34.0f, 164.0f, -3030.0f, -2390.0f},
}};

inline const Site* at(float x, float y, float z)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        return nullptr;
    for (const auto& site : kSites)
        if (x >= site.minX && x <= site.maxX &&
            y >= site.minY && y <= site.maxY &&
            z >= site.minZ && z <= site.maxZ)
            return &site;
    return nullptr;
}
} // namespace nx3d::bomb_sites

#endif
