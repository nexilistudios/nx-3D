#include <client/gamemode/player_hit_test.hh>
#include <shared/nx_objects.hh>

#include <cmath>
#include <stdexcept>

int main()
{
    using nx3d::client::gamemode::playerHitDistance;
    using nx3d::playerHit;
    const glm::vec3 player(0.0f);
    const glm::vec3 forward(0.0f, 0.0f, -1.0f);

    if (std::abs(playerHitDistance({0.0f, 0.0f, 30.0f}, forward, player, 0.0f) - 22.0f) > 0.001f)
        throw std::runtime_error("Forward shot missed the player");
    if (!playerHit({0.0f, 0.0f, 30.0f}, forward, player, 0.0f).head ||
        playerHit({0.0f, -25.0f, 30.0f}, forward, player, 0.0f).head ||
        !playerHit({0.0f, 0.0f, 30.0f}, forward, player, 0.0f,
                   nx3d::kCrouchingEyeHeight).head)
        throw std::runtime_error("Head and body hits were not distinguished");
    for (float faceHeight : {-3.0f, -6.0f})
    {
        if (!playerHit({0.0f, faceHeight, -30.0f}, -forward, player, 0.0f).head ||
            !playerHit({0.0f, faceHeight, 30.0f}, forward, player, 0.0f).head)
            throw std::runtime_error("Face or lower head did not register a headshot");
    }
    if (playerHit({0.0f, -9.0f, -30.0f}, -forward, player, 0.0f).head ||
        playerHit({0.0f, -9.0f, 30.0f}, forward, player, 0.0f).head)
        throw std::runtime_error("Throat registered as a headshot");
    for (const auto* pistol : {"glock", "usp", "deagle"})
        if (nx3d::findNXObject(pistol)->combat.headDamage != 100.0f)
            throw std::runtime_error("Pistol head damage is incorrect");
    for (const auto* rifle : {"ak47", "m16"})
        if (nx3d::findNXObject(rifle)->combat.headDamage != 200.0f)
            throw std::runtime_error("Rifle head damage is incorrect");
    if (nx3d::findNXObject("mp5")->combat.headDamage != 100.0f ||
        nx3d::findNXObject("launcher")->combat.headDamage != 0.0f)
        throw std::runtime_error("SMG or launcher head damage is incorrect");
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
