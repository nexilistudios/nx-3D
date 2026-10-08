#include <client/gun/weapon_motion.hh>

#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main()
{
    using nx3d::client::gun::WeaponMotion;
    try
    {
        for (int fps : {30, 60, 144})
        {
            WeaponMotion motion;
            motion.kick(0.1f);
            for (int frame = 0; frame < fps; ++frame)
                motion.update(1.0f / fps, {0.0f, 10.0f, 0.0f}, true);
            require(glm::length(motion.bob) == 0.0f, "Vertical jitter caused weapon bob");
            require(std::abs(motion.recoil - 0.1f * std::exp(-10.0f)) < 0.000001f,
                    "Recoil recovery depends on frame rate");

            for (int frame = 0; frame < fps * 2; ++frame)
            {
                motion.update(1.0f / fps, {250.0f, 0.0f, 0.0f}, true);
                require(std::abs(motion.bob.x) <= 0.0041f && std::abs(motion.bob.y) <= 0.0031f,
                        "Walking motion exceeds its bounds");
            }
            require(std::abs(motion.phase) < 0.0001f || std::abs(motion.phase - 6.2831853f) < 0.0001f,
                    "Walking cadence is not three cycles in two seconds");
            for (int frame = 0; frame < fps; ++frame)
                motion.update(1.0f / fps, {0.2f, 0.0f, 0.1f}, true);
            require(glm::length(motion.bob) < 0.000001f, "Weapon fails to settle at rest");
        }
        WeaponMotion motion;
        motion.update(1.0f / 60.0f, {250.0f, 0.0f, 0.0f}, false);
        motion.update(1.0f / 60.0f, {100000.0f, 0.0f, 0.0f}, true);
        require(glm::length(motion.bob) == 0.0f, "Airborne motion or teleport causes bob");
        motion.update(std::numeric_limits<float>::quiet_NaN(), {}, true);
        motion.kick(std::numeric_limits<float>::infinity());
        require(std::isfinite(motion.recoil) && std::isfinite(motion.phase), "Invalid input poisons animation");
        std::cout << "Weapon motion checks passed at 30, 60 and 144 FPS\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
