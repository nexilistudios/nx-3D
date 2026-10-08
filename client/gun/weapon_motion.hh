#ifndef NX3D_CLIENT_GUN_WEAPON_MOTION_HH
#define NX3D_CLIENT_GUN_WEAPON_MOTION_HH

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace nx3d::client::gun
{

// Presentation state only: update once per simulation frame, never per draw.
struct WeaponMotion
{
    float phase = 0.0f;
    glm::vec3 bob{0.0f};
    float recoil = 0.0f;

    void update(float dt, const glm::vec3& velocity, bool grounded)
    {
        if (!std::isfinite(dt) || dt <= 0.0f)
            return;
        const float speed = glm::length(glm::vec2(velocity.x, velocity.z));
        glm::vec3 target(0.0f);
        // Ignore ground-contact noise, vertical motion and teleport velocities.
        if (grounded && std::isfinite(speed) && speed > 20.0f && speed < 500.0f)
        {
            constexpr float tau = 6.28318530718f;
            const float pace = std::min(speed / 250.0f, 1.0f);
            phase = std::fmod(phase + dt * tau * 1.5f * pace, tau);
            target.x = std::sin(phase) * 0.004f * pace;
            target.y = std::cos(phase * 2.0f) * 0.003f * pace;
        }
        bob += (target - bob) * (1.0f - std::exp(-12.0f * dt));
        recoil *= std::exp(-10.0f * dt);
    }

    void kick(float amount)
    {
        if (std::isfinite(amount) && amount > 0.0f)
            recoil = std::min(recoil + amount, 0.18f);
    }
};

} // namespace nx3d::client::gun

#endif
