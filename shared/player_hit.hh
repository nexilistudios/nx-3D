#ifndef NX3D_SHARED_PLAYER_HIT_HH
#define NX3D_SHARED_PLAYER_HIT_HH

#include <shared/player_rules.hh>

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace nx3d
{

namespace detail
{
inline float rayBoxDistance(const glm::vec3& origin, const glm::vec3& direction,
                            const glm::vec3& boxMin, const glm::vec3& boxMax)
{
    float nearDistance = -std::numeric_limits<float>::infinity();
    float farDistance = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::abs(direction[axis]) < 1e-8f)
        {
            if (origin[axis] < boxMin[axis] || origin[axis] > boxMax[axis])
                return std::numeric_limits<float>::infinity();
            continue;
        }
        float first = (boxMin[axis] - origin[axis]) / direction[axis];
        float second = (boxMax[axis] - origin[axis]) / direction[axis];
        if (first > second)
            std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance)
            return std::numeric_limits<float>::infinity();
    }
    return farDistance >= 0.0f ? std::max(nearDistance, 0.0f)
                               : std::numeric_limits<float>::infinity();
}
} // namespace detail

// The player mesh is drawn around eye height. Its arms, torso, head and legs
// occupy different widths, so one full-width box registers hits in empty air.
// Crouching scales the rendered mesh vertically around the eye position.
inline float playerHitDistance(const glm::vec3& origin, const glm::vec3& direction,
                               const glm::vec3& playerPosition, float yaw,
                               float eyeHeight = kStandingEyeHeight)
{
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const glm::vec3 offset = origin - playerPosition;
    const glm::vec3 localOrigin(c * offset.x - s * offset.z, offset.y,
                                s * offset.x + c * offset.z);
    const glm::vec3 localDirection(c * direction.x - s * direction.z, direction.y,
                                   s * direction.x + c * direction.z);

    struct Box
    {
        glm::vec3 min, max;
    };
    constexpr std::array<Box, 8> boxes{{
            {{-8.0f, -2.0f, -8.0f}, {8.0f, 12.0f, 8.0f}},     // head
            {{-15.0f, -12.0f, -9.0f}, {15.0f, -2.0f, 9.0f}},  // shoulders
            {{-15.0f, -34.0f, -9.0f}, {15.0f, -12.0f, 9.0f}}, // torso
            {{-11.0f, -41.0f, -9.0f}, {11.0f, -34.0f, 9.0f}}, // hips
            {{-19.0f, -46.0f, -9.0f}, {-8.0f, -10.0f, 9.0f}}, // left arm
            {{8.0f, -46.0f, -9.0f}, {19.0f, -10.0f, 9.0f}},   // right arm
            {{-11.0f, -64.0f, -9.0f}, {-0.5f, -41.0f, 9.0f}}, // left leg
            {{0.5f, -64.0f, -9.0f}, {11.0f, -41.0f, 9.0f}}    // right leg
    }};
    const float heightScale = eyeHeight / kStandingEyeHeight;
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto& box : boxes)
    {
        const glm::vec3 min(box.min.x, box.min.y * heightScale, box.min.z);
        const glm::vec3 max(box.max.x, box.max.y * heightScale, box.max.z);
        nearest = std::min(nearest, detail::rayBoxDistance(localOrigin, localDirection, min, max));
    }
    return nearest;
}

} // namespace nx3d

#endif
