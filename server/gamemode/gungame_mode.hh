#ifndef NX3D_SERVER_GAMEMODE_GUNGAME_MODE_HH
#define NX3D_SERVER_GAMEMODE_GUNGAME_MODE_HH

#include <server/gamemode/deathmatch_mode.hh>

#include <mutex>
#include <unordered_map>

namespace nx3d::server::gamemode
{
class GunGameMode : public DeathmatchMode
{
public:
    explicit GunGameMode(bool shipment = false) : DeathmatchMode(shipment)
    {
    }
    nx3d::GameMode type() const override
    {
        return nx3d::GameMode::gungame;
    }
    void onTeamSelected(nexilis::server::Room& room, uint64_t id) override;
    void onPlayerDeath(nexilis::server::Room& room, uint64_t killer, uint64_t victim) override;
    void update(nexilis::server::Room& room, float dt) override;
    void onAcceptedShot(uint64_t id, const std::string& weapon) override;
    std::string weaponForSlot(uint64_t id, uint8_t slot) const override;
    bool onAction(nexilis::server::Room&, uint64_t, uint8_t) override
    {
        return false;
    }
    bool pickupWeapon(nexilis::server::Room&, uint64_t, uint8_t,
                      const std::string&) override
    {
        return false;
    }
    bool canDamage(uint64_t, uint64_t) const override
    {
        std::lock_guard lock(m_progressMutex);
        return !m_finished;
    }

protected:
    void afterRespawn(nexilis::server::Room& room, uint64_t id) override;

private:
    mutable std::mutex m_progressMutex;
    std::unordered_map<uint64_t, int> m_kills;
    std::unordered_map<uint64_t, std::string> m_lastShotWeapon;
    bool m_finished = false;
    bool m_restarting = false;
    float m_restartSeconds = 0.0f;
    int m_announcedSeconds = 0;
};
} // namespace nx3d::server::gamemode

#endif
