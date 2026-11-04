#include <client/gamemode/player_hit_test.hh>

#include <cmath>
#include <stdexcept>

int main()
{
    using nx3d::client::gamemode::playerHitDistance;
    const glm::vec3 player(0.0f);
    const glm::vec3 forward(0.0f, 0.0f, -1.0f);

    if (std::abs(playerHitDistance({0.0f, 0.0f, 30.0f}, forward, player, 0.0f) - 22.0f) > 0.001f)
        throw std::runtime_error("Forward shot missed the player");
    if (playerHitDistance({0.0f, 0.0f, 0.0f}, forward, player, 0.0f) != 0.0f)
        throw std::runtime_error("Shot from inside the hitbox missed");
    if (std::isfinite(playerHitDistance({15.0f, 0.0f, 30.0f}, forward, player, 1.5707963f)))
        throw std::runtime_error("Rotated narrow side accepted a miss");
    if (!std::isfinite(playerHitDistance({0.0f, 0.0f, 30.0f}, forward, player, 1.5707963f)))
        throw std::runtime_error("Rotated wide side missed");
    if (std::isfinite(playerHitDistance({0.0f, 0.0f, 30.0f}, -forward, player, 0.0f)))
        throw std::runtime_error("Shot behind the shooter hit");
    if (!std::isfinite(playerHitDistance({0.0f, 70.0f, 30.0f}, forward,
                                         {0.0f, 64.0f, 0.0f}, 0.0f)) ||
        std::isfinite(playerHitDistance({0.0f, 70.0f, 30.0f}, forward,
                                        {0.0f, 40.0f, 0.0f}, 0.0f, 40.0f)))
        throw std::runtime_error("Crouched player kept a standing hitbox");
    if (std::isfinite(playerHitDistance({16.0f, 5.0f, 30.0f}, forward, player, 0.0f)) ||
        std::isfinite(playerHitDistance({17.0f, -55.0f, 30.0f}, forward, player, 0.0f)))
        throw std::runtime_error("Empty space beside the head or legs registered a hit");
    if (!std::isfinite(playerHitDistance({16.0f, -25.0f, 30.0f}, forward, player, 0.0f)) ||
        !std::isfinite(playerHitDistance({6.0f, -55.0f, 30.0f}, forward, player, 0.0f)))
        throw std::runtime_error("Arm or leg hitboxes missed a visible limb");
    if (std::isfinite(playerHitDistance({0.0f, 49.0f, 30.0f}, forward,
                                        {0.0f, 40.0f, 0.0f}, 0.0f, 40.0f)))
        throw std::runtime_error("Crouched head hitbox extends above the model");
}
