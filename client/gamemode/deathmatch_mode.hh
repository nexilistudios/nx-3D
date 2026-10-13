#ifndef NX3D_CLIENT_GAMEMODE_DEATHMATCH_MODE_HH
#define NX3D_CLIENT_GAMEMODE_DEATHMATCH_MODE_HH

#include <client/crosshair/crosshair.hh>
#include <client/gamemode/game_mode.hh>
#include <client/gun/first_person_gun.hh>
#include <client/ui/ui.hh>

#include <spear/rendering/vulkan/shapes/textured_cube.hh>

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
/// de_dust2, are issued a team weapon and respawn shortly after being killed.
class DeathmatchMode : public GameMode
{
public:
    nx3d::GameMode type() const override;
    std::string playerTeam(uint64_t playerId) const override;
    void onEnter(ClientApp& app, const std::string& team) override;
    void update(ClientApp& app, float delta_time) override;
    bool canMove() const override
    {
        return m_health > 0;
    }
    bool isEliminated() const override
    {
        return m_health <= 0;
    }
    void handleMouseButtonDown(ClientApp& app, const SDL_Event& event) override;
    void handleMouseButtonUp(ClientApp& app, const SDL_Event& event) override;
    void handleKeyDown(ClientApp& app, const SDL_Event& event) override;
    void handleKeyUp(ClientApp& app, const SDL_Event& event) override;
    void changeTeam(ClientApp& app, const std::string& team) override;
    void onEconomySnapshot(ClientApp& app, const nx3d::economy::Snapshot& state) override;
    void onGrenadeThrown(ClientApp& app, const nx3d::economy::GrenadeThrow& event) override;

protected:
    void equipGun(ClientApp& app, const std::string& weaponName,
                  const std::string& objPath, const std::string& mtlPath,
                  float fpScale, glm::vec3 fpCenter,
                  glm::vec3 fpOffset, glm::vec3 fpRotation);
    void equipWeaponForTeam(ClientApp& app);
    void dropCurrentWeapon(ClientApp& app);
    void pickupNearbyWeapon(ClientApp& app);
    void fireWeapon(ClientApp& app);
    void startReload(ClientApp& app);
    void refreshWeaponHud(ClientApp& app);
    void equipSlot(ClientApp& app, int slot, bool force = false);
    void throwGrenade(ClientApp& app);
    void showGrenade(ClientApp& app, glm::vec3 start, glm::vec3 end, bool smoke);
    void installOverlay(ClientApp& app);
    float fireIntervalSeconds() const;
    glm::vec3 pickSpawnPoint();

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
    int m_health = 100;
    int m_hitmarkerFrames = 0;
    std::string m_weaponName;
    struct GunSlot
    {
        std::string type;
        int ammo = -1;
        int reserve = -1;
        uint64_t itemId = 0;
        glm::vec3 itemSize{1.0f};
    };
    GunSlot m_primary;
    GunSlot m_secondary;
    int m_activeSlot = 2;
    int m_grenades = 0;
    int m_smokes = 0;
    struct GrenadeVisual
    {
        std::shared_ptr<spear::rendering::vulkan::TexturedCube> model;
        glm::vec3 start;
        glm::vec3 end;
        float age = 0.0f;
        bool smoke = false;
        std::vector<std::shared_ptr<spear::rendering::vulkan::TexturedCube>> puffs;
    };
    std::vector<GrenadeVisual> m_grenadeVisuals;
    int m_ammo = 0;
    int m_reserveAmmo = 0;
    int m_magazineSize = 0;
    float m_reloadRemaining = 0.0f;
    glm::vec3 m_prevCamPos{0.0f};

    // Automatic fire state
    bool m_triggerHeld = false;
    float m_fireCooldown = 0.0f;
    /// Consecutive shots in the current burst; each shot kicks harder than the
    /// last. Reset when the trigger is released.
    int m_burstShots = 0;

    // Index of the last spawn point used, so a respawn never lands on the
    // same spawn the player died at.
    size_t m_lastSpawnIndex = SIZE_MAX;

};

} // namespace nx3d::client::gamemode

#endif
