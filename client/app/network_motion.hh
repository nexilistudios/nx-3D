#ifndef NX3D_CLIENT_APP_NETWORK_MOTION_HH
#define NX3D_CLIENT_APP_NETWORK_MOTION_HH

#include <glm/glm.hpp>

#include <cmath>

namespace nx3d::client
{

// Smooth presentation between server position updates. A large correction is
// a spawn or teleport, so show it immediately instead of sliding across the map.
inline glm::vec3 smoothRemotePosition(const glm::vec3& rendered,
                                      const glm::vec3& authoritative, float dt)
{
    constexpr float kSmoothingSeconds = 0.035f;
    constexpr float kSnapDistance = 120.0f;
    if (glm::distance(rendered, authoritative) >= kSnapDistance)
        return authoritative;
    if (dt <= 0.0f)
        return rendered;
    const float blend = 1.0f - std::exp(-dt / kSmoothingSeconds);
    return glm::mix(rendered, authoritative, blend);
}

inline float smoothRemoteFacing(float rendered, float authoritative, float dt)
{
    constexpr float kSmoothingSeconds = 0.025f;
    constexpr float kFullTurn = 6.28318530718f;
    if (dt <= 0.0f)
        return rendered;
    const float delta = std::remainder(authoritative - rendered, kFullTurn);
    return rendered + delta * (1.0f - std::exp(-dt / kSmoothingSeconds));
}

// Local movement is predicted by MovementController each frame. Correct small
// differences only when the server advances or the player is resting; pulling
// toward a stale packet while running would recreate the visible stop and go.
inline glm::vec3 reconcileLocalPosition(const glm::vec3& predicted,
                                        const glm::vec3& authoritative, float dt,
                                        bool freshOrResting, bool teleported = false)
{
    constexpr float kCorrectionSeconds = 0.08f;
    if (teleported)
        return authoritative;
    if (!freshOrResting || dt <= 0.0f)
        return predicted;
    const float blend = 1.0f - std::exp(-dt / kCorrectionSeconds);
    return glm::mix(predicted, authoritative, blend);
}

} // namespace nx3d::client

#endif
