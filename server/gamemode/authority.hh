#ifndef NX3D_SERVER_GAMEMODE_AUTHORITY_HH
#define NX3D_SERVER_GAMEMODE_AUTHORITY_HH

#include <nexilis/server/room.hh>
#include <shared/bullet_hole.hh>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace nx3d::server::gamemode
{

/// Receives player intent and applies movement and shots using server state.
class Authority
{
public:
    struct DroppedWeapon
    {
        std::string type;
        int magazine;
        int reserve;
    };
    explicit Authority(bool shipment = false);
    void spawn(nexilis::server::Room& room, uint64_t playerId,
               bool preserveEquipment = false, bool gunGame = false);
    bool move(nexilis::server::Room& room, uint64_t playerId,
              const nexilis::Vector3f& input, float yaw);
    void update(nexilis::server::Room& room, float dt);
    DroppedWeapon dropWeapon(uint64_t playerId, uint8_t slot, const std::string& type);
    void pickupWeapon(uint64_t playerId, uint8_t slot, const DroppedWeapon& weapon);
    bool equipWeapon(nexilis::server::Room& room, uint64_t playerId,
                     const std::string& weapon);
    void refill(uint64_t playerId);
    std::optional<nexilis::server::Room::ShotHit> fire(
            nexilis::server::Room& room, uint64_t shooterId,
            const nexilis::Vector3f& direction, uint8_t slot, uint8_t action,
            const std::string& weapon, const std::function<void()>& onAcceptedShot = {});
    void reconcile(const nexilis::server::Room& room);
    void sendBulletHoles(nexilis::server::Room& room, uint64_t playerId);
    void resetBulletHoles(nexilis::server::Room& room);

private:
    bool m_shipment = false;
    struct WeaponState
    {
        std::string type;
        int magazine = 0;
        int reserve = 0;
        std::chrono::steady_clock::time_point lastShot{};
        std::chrono::steady_clock::time_point reloadEnd{};
    };
    struct PlayerState
    {
        std::chrono::steady_clock::time_point lastInput{};
        nexilis::Vector3f input{};
        float yaw = 0.0f;
        float verticalVelocity = 0.0f;
        bool grounded = false;
        bool jumpHeld = false;
        bool crouched = false;
        std::string animation = "idle";
        std::string activeWeapon;
        std::chrono::steady_clock::time_point lastAnimationBroadcast{};
        WeaponState primary;
        WeaponState secondary;
        WeaponState knife;
    };

    struct FlyingGrenade
    {
        uint64_t owner;
        std::string team;
        float x, y, z;
        float seconds;
    };

    std::mutex m_mutex;
    std::unordered_map<uint64_t, PlayerState> m_players;
    std::vector<FlyingGrenade> m_flyingGrenades;
    std::deque<nx3d::BulletHole> m_bulletHoles;
    uint64_t m_nextBulletHoleId = 1;
    uint64_t m_spawnCounter = 0;
    std::mt19937 m_spawnRandom{std::random_device{}()};
    std::unordered_map<uint64_t, std::size_t> m_lastGunGameSpawn;
};

} // namespace nx3d::server::gamemode

#endif
