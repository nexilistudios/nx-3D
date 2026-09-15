#ifndef NX3D_CLIENT_GAMEMODE_DEATHMATCH_MODE_HH
#define NX3D_CLIENT_GAMEMODE_DEATHMATCH_MODE_HH

#include <client/crosshair/crosshair.hh>
#include <client/gamemode/game_mode.hh>
#include <client/gun/first_person_gun.hh>
#include <client/ui/ui.hh>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nx3d::client::gamemode
{

/// Classic team deathmatch.
///
/// Players pick a team in the team select screen, spawn on that team's side of
/// de_dust2, are issued a team weapon and are instantly respawned when killed.
class DeathmatchMode : public GameMode
{
public:
    nx3d::GameMode type() const override;
    void onEnter(ClientApp& app, const std::string& team) override;
    void update(ClientApp& app, float delta_time) override;
    void handleMouseButtonDown(ClientApp& app, const SDL_Event& event) override;
    void handleKeyDown(ClientApp& app, const SDL_Event& event) override;
    void handleKeyUp(ClientApp& app, const SDL_Event& event) override;

private:
    void equipGun(ClientApp& app, const std::string& weaponName,
                  const std::string& objPath, const std::string& mtlPath,
                  float fpScale, glm::vec3 fpCenter);
    void unequipGun(ClientApp& app);
    void equipWeaponForTeam(ClientApp& app);
    void dropCurrentWeapon(ClientApp& app);

    std::string m_team;

    // Locally-maintained kill/death table, keyed by player id. Updated from
    // server stats notifications (on kill and on team join); the Tab board is
    // rendered straight from here without asking the server.
    std::unordered_map<uint64_t, nx3d::client::ui::LeaderboardEntry> m_leaderboard;

    // Weapon state
    std::shared_ptr<nx3d::client::gun::FirstPersonGun> m_firstPersonGun;
    std::shared_ptr<nx3d::client::Crosshair> m_crosshair;
    bool m_weaponPickedUp = false;
    uint64_t m_pickedUpItemId = 0;
    glm::vec3 m_pickedUpItemSize{1.0f};
    std::string m_pickedUpItemType;
    std::string m_pickedUpItemFilepath;
    int m_dropCooldown = 0;
    int m_health = 100;
    int m_hitmarkerFrames = 0;
    glm::vec3 m_prevCamPos{0.0f};

    // Team spawn points (de_dust2 coordinates).
    std::vector<glm::vec3> m_ctSpawns{
        {355.0f, -64.0f, -2364.0f},
        {178.0f, -64.0f, -2408.0f}
    };
    std::vector<glm::vec3> m_tSpawns{
        {-592.0f, 192.0f, 764.0f},
        {-834.0f, 192.0f, 797.0f}
    };
};

} // namespace nx3d::client::gamemode

#endif