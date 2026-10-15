#ifndef NX3D_CLIENT_GUN_WEAPON_MOTION_HH
#define NX3D_CLIENT_GUN_WEAPON_MOTION_HH

#include <shared/player_rules.hh>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace nx3d::client::gun
{

// The magazine exits, pauses briefly, then seats before the weapon settles.
inline float magazineTravel(float progress)
{
    if (progress < 0.18f)
        return std::clamp(progress / 0.18f, 0.0f, 1.0f);
    if (progress < 0.58f)
        return 1.0f;
    return std::clamp((0.84f - progress) / 0.26f, 0.0f, 1.0f);
}

// Presentation state only: update once per simulation frame, never per draw.
struct WeaponMotion
{
    float phase = 0.0f;
    glm::vec3 bob{0.0f};
    float recoil = 0.0f;
    float reload_elapsed = 0.0f;
    float reload_duration = 0.0f;
    glm::vec3 reload_offset{0.0f};
    float reload_tilt = 0.0f;

    float magazineOffset() const
    {
        return reload_duration > 0.0f
                       ? magazineTravel(reload_elapsed / reload_duration) : 0.0f;
    }

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
            const float pace = std::min(speed / nx3d::kWalkSpeed, 1.0f);
            phase = std::fmod(phase + dt * tau * 1.5f * pace, tau);
            target.x = std::sin(phase) * 0.004f * pace;
            target.y = std::cos(phase * 2.0f) * 0.003f * pace;
        }
        bob += (target - bob) * (1.0f - std::exp(-12.0f * dt));
        recoil *= std::exp(-10.0f * dt);

        if (reload_duration > 0.0f)
        {
            if (dt >= reload_duration - reload_elapsed - 0.00001f)
                reload_elapsed = reload_duration;
            else
                reload_elapsed += dt;
            constexpr float pi = 3.14159265359f;
            const float progress = reload_elapsed / reload_duration;
            const float pose = std::sin(pi * progress);
            reload_offset = {0.05f * pose, -0.24f * pose, 0.06f * pose};
            reload_tilt = -18.0f * pose;
            if (reload_elapsed >= reload_duration)
            {
                reload_duration = 0.0f;
                reload_offset = {};
                reload_tilt = 0.0f;
            }
        }
    }

    void kick(float amount)
    {
        if (std::isfinite(amount) && amount > 0.0f)
            recoil = std::min(recoil + amount, 0.18f);
    }

    void startReload(float duration)
    {
        if (!std::isfinite(duration) || duration <= 0.0f)
            return;
        reload_elapsed = 0.0f;
        reload_duration = duration;
        reload_offset = {};
        reload_tilt = 0.0f;
    }
};

} // namespace nx3d::client::gun

#endif
