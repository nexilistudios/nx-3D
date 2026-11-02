#ifndef NX3D_SERVER_GAMEMODE_DEATHMATCH_MODE_HH
#define NX3D_SERVER_GAMEMODE_DEATHMATCH_MODE_HH

#include <server/gamemode/game_mode.hh>
#include <server/gamemode/economy.hh>

#include <mutex>
#include <unordered_map>

namespace nx3d::server::gamemode
{

/// Classic deathmatch.
///
/// Players respawn after a short death screen.
class DeathmatchMode : public GameMode
{
public:
    explicit DeathmatchMode(bool shipment = false) : m_economy(shipment) {}
    nx3d::GameMode type() const override;
    void onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId) override;
    void onTeamSelected(nexilis::server::Room& room, uint64_t playerId) override;
    bool onAction(nexilis::server::Room& room, uint64_t playerId, uint8_t action) override;
    std::string weaponForSlot(uint64_t playerId, uint8_t slot) const override
    {
        return m_economy.weaponForSlot(playerId, slot);
    }
    bool pickupWeapon(nexilis::server::Room& room, uint64_t id, uint8_t slot,
                      const std::string& weapon) override
    {
        return m_economy.pickupWeapon(room, id, slot, weapon);
    }
    void update(nexilis::server::Room& room, float dt) override;

private:
    std::mutex m_respawnMutex;
    std::unordered_map<uint64_t, float> m_respawnTimers;
    Economy m_economy;
};

} // namespace nx3d::server::gamemode

#endif
