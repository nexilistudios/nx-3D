#include <client/app/client_app.hh>
#include <client/gamemode/deathmatch_mode.hh>
#include <client/gamemode/player_hit_test.hh>
#include <client/gamemode/weapon_profiles.hh>
#include <shared/ammunition.hh>

#include <nexilis/client/packet.hh>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace nx3d::client::gamemode
{

namespace blt = spear::physics::bullet;

namespace
{
/// Recoil ramps up as a burst goes on: each shot multiplies the previous
/// shot's view/gun punch slightly more, capped so sustained fire stays
/// controllable. Reset on trigger release.
constexpr float kRecoilStartMultiplier = 0.6f;
constexpr float kRecoilStep = 0.15f;
constexpr float kMaxRecoilMultiplier = 2.5f;

class MapRayResultCallback : public btCollisionWorld::ClosestRayResultCallback
{
public:
    MapRayResultCallback(const btVector3& from, const btVector3& to,
                         const btCollisionObject* mapBody)
        : btCollisionWorld::ClosestRayResultCallback(from, to),
          m_mapBody(mapBody)
    {
    }

    btScalar addSingleResult(btCollisionWorld::LocalRayResult& result,
                             bool normalInWorldSpace) override
    {
        if (result.m_collisionObject != m_mapBody)
            return 1.0f;
        return ClosestRayResultCallback::addSingleResult(result, normalInWorldSpace);
    }

private:
    const btCollisionObject* m_mapBody;
};

bool mapBlocksShot(const ClientApp& app, const glm::vec3& origin,
                   const glm::vec3& direction, float targetDistance)
{
    if (!app.mapCollisionBody || !app.shared_world || !std::isfinite(targetDistance))
        return false;

    const glm::vec3 target = origin + direction * targetDistance;
    const btVector3 from(origin.x, origin.y, origin.z);
    const btVector3 to(target.x, target.y, target.z);
    MapRayResultCallback hit(from, to, app.mapCollisionBody);
    app.shared_world->rayTest(from, to, hit);
    return hit.hasHit() && hit.m_closestHitFraction < 1.0f;
}
} // namespace

nx3d::GameMode DeathmatchMode::type() const
{
    return nx3d::GameMode::deathmatch;
}

std::string DeathmatchMode::playerTeam(uint64_t playerId) const
{
    const auto it = m_leaderboard.find(playerId);
    return it == m_leaderboard.end() ? std::string{} : it->second.team;
}

void DeathmatchMode::onEnter(ClientApp& app, const std::string& team)
{
    using packet = nexilis::client::Packet;

    m_team = team;
    app.ui->teamDisplayText.setString(m_team);

    app.ui->hideLeaderboard();
    m_leaderboard.clear();

    const auto& spawns = (m_team == "Terrorist") ? nx3d::economy::kTSpawns : nx3d::economy::kCtSpawns;
    m_lastSpawnIndex = SIZE_MAX;
    if (!spawns.empty())
    {
        app.camera.setPosition(pickSpawnPoint());
        app.movement_controller.resetVerticalMotion();
    }

    // A previous fall can leave the server's stored position below the map.
    // Send the new spawn over TCP before selecting a team, because selecting
    // a team makes the player eligible for the server's fall-death check.
    const glm::vec3 spawn = app.camera.getPosition();
    const glm::vec3 forward = app.camera.getFront();
    const float yaw = std::atan2(-forward.x, -forward.z);
    app.tcp_client.sendMessage(packet::Room::Player3D::position(
            app.client_api, nexilis::Vector3f({spawn.x, spawn.y, spawn.z}), yaw));

    // The server replies with a stats seed for the local leaderboard.
    app.tcp_client.sendMessage(packet::Room::Player3D::setTeam(app.client_api, team));

    m_health = 100;
    app.ui->healthText.setString("HP: " + std::to_string(m_health));
    app.ui->wastedText.setString("");
    m_prevCamPos = app.camera.getPosition();

    m_weaponPickedUp = true;
    equipWeaponForTeam(app);
}

void DeathmatchMode::update(ClientApp& app, float delta_time)
{
    using packet = nexilis::client::Packet;

    m_fireCooldown = std::max(0.0f, m_fireCooldown - delta_time);
    for (auto it = m_grenadeVisuals.begin(); it != m_grenadeVisuals.end();)
    {
        it->age += delta_time;
        if (it->age >= nx3d::economy::kGrenadeFuseSeconds && it->model)
        {
            if (!it->smoke && app.explosion_audio)
                app.explosion_audio->play();
            if (it->smoke && app.smoke_audio)
                app.smoke_audio->playAt(it->end);
            app.scene_manager.getCurrentScene()->removeObject(it->model->getId());
            app.pendingDestroy[(app.frameCount - 1) % 3].push_back(std::move(it->model));
            if (it->smoke)
            {
                for (int puff = 0; puff < 16; ++puff)
                {
                    const float angle = static_cast<float>(puff % 8) * 0.78539816f;
                    const float radius = puff < 8 ? 34.0f : 59.0f;
                    const float height = puff < 8 ? -13.0f : 16.0f;
                    auto cloud = std::make_shared<spear::rendering::vulkan::TexturedCube>(
                            app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                            app.smoke_texture, app.descriptorPool, app.descriptorSetLayout,
                            blt::ObjectData(app.shared_world, 0.0f, it->end,
                                            app.default_size));
                    cloud->scale(glm::vec3(27.0f, 32.0f, 27.0f));
                    cloud->setPosition(it->end +
                                       glm::vec3(std::cos(angle) * radius, height,
                                                 std::sin(angle) * radius));
                    app.scene_manager.getCurrentScene()->addObject(cloud);
                    it->puffs.push_back(std::move(cloud));
                }
            }
        }
        if (it->model)
        {
            const float progress = it->age / nx3d::economy::kGrenadeFuseSeconds;
            const glm::vec3 position = it->start + (it->end - it->start) * progress +
                                       glm::vec3(0.0f, std::sin(progress * 3.14159265f) * 55.0f, 0.0f);
            it->model->setPosition({position.x, position.y, position.z});
        }
        else if (it->smoke)
        {
            const float cloudAge = it->age - nx3d::economy::kGrenadeFuseSeconds;
            for (std::size_t puff = 0; puff < it->puffs.size(); ++puff)
            {
                const float angle = static_cast<float>(puff % 8) * 0.78539816f;
                const float radius = puff < 8 ? 34.0f : 59.0f;
                const float height = puff < 8 ? -13.0f : 16.0f;
                it->puffs[puff]->setPosition(it->end + glm::vec3(
                        std::cos(angle) * radius + std::sin(cloudAge * 0.65f + angle) * 5.0f,
                        height + std::sin(cloudAge * 0.8f + angle) * 4.0f,
                        std::sin(angle) * radius + std::cos(cloudAge * 0.55f + angle) * 5.0f));
            }
        }
        if (!it->model && (!it->smoke || it->age >= nx3d::economy::kGrenadeFuseSeconds +
                                                   nx3d::economy::kSmokeDurationSeconds))
        {
            for (auto& puff : it->puffs)
            {
                app.scene_manager.getCurrentScene()->removeObject(puff->getId());
                app.pendingDestroy[(app.frameCount - 1) % 3].push_back(std::move(puff));
            }
            it = m_grenadeVisuals.erase(it);
            continue;
        }
        ++it;
    }

    if (m_reloadRemaining > 0.0f)
    {
        m_reloadRemaining = std::max(0.0f, m_reloadRemaining - delta_time);
        if (m_reloadRemaining == 0.0f)
        {
            nx3d::ammunition::reload(m_ammo, m_reserveAmmo, m_magazineSize);
            refreshWeaponHud(app);
        }
    }

    if (m_hitmarkerFrames > 0)
    {
        m_hitmarkerFrames--;
        app.ui->hitmarkerText.setString(m_hitmarkerFrames > 0 ? "X" : "");
    }
    // Play the hitmarker sound shortly after a hit (handled in the shared
    // GameMode base so it works in every gamemode).
    updateHitmarkerSound(app);

    // --- Gun walk-bob ---
    glm::vec3 cam_pos = app.camera.getPosition();
    glm::vec3 velocity = (cam_pos - m_prevCamPos) / std::max(delta_time, 0.001f);
    m_prevCamPos = cam_pos;

    // Start/stop the footstep loop based on how fast the player is actually
    // moving across the ground (horizontal only, so falling stays quiet).
    updateWalkSound(app, std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z),
                    delta_time);

    // Spatialize sounds other players reported (gunshots and footsteps).
    updateRemoteAudioEvents(app);

    if (m_firstPersonGun)
        m_firstPersonGun->updateAnimation(delta_time, velocity, app.movement_controller.isOnGround());

    // --- Automatic fire while the trigger is held ---
    if (m_health > 0 && m_triggerHeld && m_weaponPickedUp &&
        getWeaponProfile(m_pickedUpItemType).automatic && m_fireCooldown <= 0.0f)
    {
        fireWeapon(app);
        m_fireCooldown = fireIntervalSeconds();
    }

    // --- Consume damage events ---
    auto damageEvents = app.client_api.consumeDamageEvents();
    for (auto& evt : damageEvents)
    {
        if (evt.target_id == app.client_api.getClientId())
        {
            m_health = static_cast<int>(evt.new_health);
            if (m_health < 0)
                m_health = 0;
            app.ui->healthText.setString("HP: " + std::to_string(m_health));
            if (m_health == 0)
            {
                m_triggerHeld = false;
                app.movement_controller.resetVerticalMotion();
            }
            app.camera.addRecoilOffset(m_health == 0 ? -7.0f : -1.5f, 0.0f);
        }
        else if (evt.shooter_id == app.client_api.getClientId() && evt.damage > 0.0f)
        {
            m_hitmarkerFrames = 15;
            triggerHitmarkerSound();
        }
        if (evt.target_id != app.client_api.getClientId())
        {
            if (evt.new_health <= 0.0f)
                app.animatePlayerDeath(evt.target_id);
            else
                app.animatePlayerHit(evt.target_id);
        }
    }

    // --- Consume respawn events ---
    auto respawnEvents = app.client_api.consumeRespawnEvents();
    for (auto& evt : respawnEvents)
    {
        if (evt.target_id == app.client_api.getClientId())
        {
            const auto& spawns = (m_team == "Terrorist") ? nx3d::economy::kTSpawns : nx3d::economy::kCtSpawns;
            if (!spawns.empty())
            {
                app.camera.setPosition(pickSpawnPoint());
                app.movement_controller.resetVerticalMotion();
            }
            m_prevCamPos = app.camera.getPosition();
            m_triggerHeld = false;
            m_burstShots = 0;
            m_fireCooldown = 0.0f;
            if (m_firstPersonGun)
                m_firstPersonGun->resetAnimation();
            m_health = 100;
            app.ui->healthText.setString("HP: " + std::to_string(m_health));
            app.ui->wastedText.setString("");
            m_reloadRemaining = 0.0f;
            if (app.reload_audio)
                app.reload_audio->stop();
            refreshWeaponHud(app);

            equipWeaponForTeam(app);
        }
        else
        {
            app.animatePlayerRespawn(evt.target_id);
        }
    }

    // --- Merge leaderboard stats updates into our local table ---
    auto leaderboardEvents = app.client_api.consumeLeaderboardEvents();
    for (auto& evt : leaderboardEvents)
    {
        for (auto& entry : evt.entries)
        {
            if (entry.id == 0)
                continue;
            m_leaderboard[entry.id] =
                    {entry.id, entry.username, entry.team, entry.kills, entry.deaths};
        }

        // If the Tab board is currently open, refresh it from the local table
        // only (no network round-trip required to show up-to-date numbers).
        if (app.ui->isLeaderboardVisible())
        {
            std::vector<nx3d::client::ui::LeaderboardEntry> uiEntries;
            uiEntries.reserve(m_leaderboard.size());
            for (auto& [id, entry] : m_leaderboard)
                uiEntries.push_back(entry);
            app.ui->showLeaderboard(uiEntries);
        }
    }
}

void DeathmatchMode::handleMouseButtonDown(ClientApp& app, const SDL_Event& event)
{
    if (event.button.button == SDL_BUTTON_LEFT && m_health > 0 &&
        (m_activeSlot == 4 || m_activeSlot == 5))
    {
        throwGrenade(app);
        return;
    }
    if (m_health <= 0 || !m_weaponPickedUp || event.button.button != SDL_BUTTON_LEFT)
        return;
    if (m_reloadRemaining > 0.0f || (m_ammo == 0 && m_activeSlot != 3) ||
        m_fireCooldown > 0.0f)
        return;

    // Start a fresh burst: the first shot fires immediately, subsequent shots
    // are paced by the weapon's rate of fire while the trigger is held.
    m_triggerHeld = getWeaponProfile(m_pickedUpItemType).automatic;
    m_burstShots = 0;
    fireWeapon(app);
    m_fireCooldown = fireIntervalSeconds();
}

void DeathmatchMode::handleMouseButtonUp(ClientApp& app, const SDL_Event& event)
{
    if (event.button.button != SDL_BUTTON_LEFT)
        return;

    m_triggerHeld = false;
    m_burstShots = 0;
}

float DeathmatchMode::fireIntervalSeconds() const
{
    return 60.0f / getWeaponProfile(m_pickedUpItemType).rounds_per_minute;
}

void DeathmatchMode::fireWeapon(ClientApp& app)
{
    using packet = nexilis::client::Packet;

    if (m_activeSlot == 3)
    {
        const glm::vec3 origin = app.camera.getPosition();
        const glm::vec3 direction = glm::normalize(app.camera.getFront());
        const auto players = app.client_api.getRemotePlayersSnapshot(
                app.client_api.clientRoomId(), app.client_api.getClientId());
        float closest = 100.0f;
        uint64_t target = 0;
        for (const auto& player : players)
        {
            if (app.remote_dead_players.contains(player.id)) continue;
            const float distance = playerHitDistance(origin, direction,
                                                     {player.x, player.y, player.z}, player.yaw);
            if (distance < closest)
            {
                closest = distance;
                target = player.id;
            }
        }
        if (target && !mapBlocksShot(app, origin, direction, closest))
            app.tcp_client.sendMessage(packet::Room::Player3D::shoot(app.client_api, target, 35.0f));
        return;
    }

    if (m_reloadRemaining > 0.0f || m_ammo == 0)
    {
        m_triggerHeld = false;
        return;
    }
    --m_ammo;
    refreshWeaponHud(app);

    // Recoil grows the longer a burst runs: each shot kicks harder than the
    // last, resetting only when the trigger is released.
    m_burstShots++;
    const float recoilMul = std::min(
            kRecoilStartMultiplier + static_cast<float>(m_burstShots) * kRecoilStep,
            kMaxRecoilMultiplier);

    const uint8_t shotSound = audio::shotSoundForWeapon(m_pickedUpItemType);
    if (auto* sound = app.gunshotSound(shotSound))
        sound->play();

    // Tell every other client where this shot happened so they can play the
    // gunshot sound spatically. Only sent once per trigger: the shoot packet
    // below carries the damage, this one carries the audio origin.
    if (app.ready && app.client_api.clientInRoom())
    {
        glm::vec3 cam_pos = app.camera.getPosition();
        app.tcp_client.sendMessage(packet::Room::Player3D::audioEvent(
                app.client_api, shotSound,
                nexilis::Vector3f({cam_pos.x, cam_pos.y, cam_pos.z})));
    }

    // Capture the aim direction BEFORE the aim punch is applied, so the
    // bullet goes exactly where the crosshair points when firing. The punch
    // below is purely visual feedback and decays after the shot.
    glm::vec3 rayOrigin = app.camera.getPosition();
    glm::vec3 rayDir = glm::normalize(app.camera.getFront());

    if (m_firstPersonGun)
        m_firstPersonGun->addRecoil(0.1f * recoilMul);

    // CS-style aim punch: kick the view up with a little random yaw,
    // decaying over time. Scales up with burst length.
    float yawKick = (static_cast<float>(rand() % 100) - 50.0f) / 50.0f * 0.6f * recoilMul;
    app.camera.addRecoilOffset(1.4f * recoilMul, yawKick);

    auto room_id = app.client_api.clientRoomId();
    auto my_id = app.client_api.getClientId();
    auto players = app.client_api.getRemotePlayersSnapshot(room_id, my_id);

    float closestDist = std::numeric_limits<float>::max();
    uint64_t hitTargetId = 0;

    for (auto& player : players)
    {
        if (app.remote_dead_players.contains(player.id))
            continue;
        const float distance = playerHitDistance(rayOrigin, rayDir,
                                                 {player.x, player.y, player.z}, player.yaw);
        if (distance < closestDist)
        {
            closestDist = distance;
            hitTargetId = player.id;
        }
    }

    if (hitTargetId != 0 && !mapBlocksShot(app, rayOrigin, rayDir, closestDist))
    {
        app.tcp_client.sendMessage(
                packet::Room::Player3D::shoot(app.client_api, hitTargetId,
                                              getWeaponProfile(m_pickedUpItemType).damage));
    }
}

void DeathmatchMode::handleKeyDown(ClientApp& app, const SDL_Event& event)
{
    // Ignore OS auto-repeat: holding Tab/G must only trigger the action once.
    if (event.key.repeat)
        return;

    if (m_health > 0 && event.key.key >= SDLK_1 && event.key.key <= SDLK_5)
        equipSlot(app, static_cast<int>(event.key.key - SDLK_0));
    else if (event.key.key == SDLK_R && m_health > 0 && m_weaponPickedUp && m_activeSlot <= 2)
        startReload(app);
    else if (event.key.key == SDLK_G && m_health > 0 && m_weaponPickedUp && m_activeSlot <= 2)
        dropCurrentWeapon(app);
    else if (event.key.key == SDLK_F && m_health > 0)
        pickupNearbyWeapon(app);
    else if (event.key.key == SDLK_TAB && app.client_api.clientInRoom())
    {
        // Render the locally-maintained leaderboard. No packet is sent: the
        // server keeps clients up to date with kill/team stats as they happen.
        std::vector<nx3d::client::ui::LeaderboardEntry> uiEntries;
        uiEntries.reserve(m_leaderboard.size());
        for (auto& [id, entry] : m_leaderboard)
            uiEntries.push_back(entry);
        app.ui->showLeaderboard(uiEntries);
    }
}

void DeathmatchMode::changeTeam(ClientApp& app, const std::string& team)
{
    if (team == m_team)
        return;

    m_team = team;
    app.ui->teamDisplayText.setString(m_team);
    app.tcp_client.sendMessage(
            nexilis::client::Packet::Room::Player3D::setTeam(app.client_api, team));

    // Issue the selected side's standard weapon immediately. The player's
    // position and life state are retained; their next respawn uses the new
    // team's spawn points.
    m_weaponPickedUp = true;
    equipWeaponForTeam(app);
}

void DeathmatchMode::onEconomySnapshot(ClientApp& app, const nx3d::economy::Snapshot& state)
{
    auto syncSlot = [](GunSlot& slot, const std::string& type)
    {
        if (type.empty() && slot.itemId != 0)
            return;
        if (slot.type != type)
        {
            slot.type = type;
            slot.ammo = -1;
            slot.reserve = -1;
            slot.itemId = 0;
            slot.itemSize = glm::vec3(1.0f);
        }
    };
    syncSlot(m_primary, state.primary);
    syncSlot(m_secondary, state.secondary);
    m_grenades = state.grenades;
    m_smokes = state.smokes;
    if (state.effect == "weapon" && hasWeaponProfile(state.weapon))
    {
        const int slot = nx3d::economy::isSecondary(state.weapon) ? 2 : 1;
        GunSlot& purchased = slot == 1 ? m_primary : m_secondary;
        if (purchased.itemId != 0 && app.ready && app.client_api.clientInRoom())
            app.tcp_client.sendMessage(nexilis::client::Packet::Room::GameItem::destroy(
                    app.client_api, purchased.itemId));
        purchased.type = state.weapon;
        purchased.ammo = -1;
        purchased.reserve = -1;
        purchased.itemId = 0;
        purchased.itemSize = glm::vec3(1.0f);
        equipSlot(app, slot, true);
    }
    else if ((m_activeSlot == 1 && m_primary.type.empty()) ||
             (m_activeSlot == 2 && m_secondary.type.empty()) ||
             (m_activeSlot == 4 && m_grenades == 0) ||
             (m_activeSlot == 5 && m_smokes == 0))
        equipSlot(app, !m_primary.type.empty() ? 1 :
                      !m_secondary.type.empty() ? 2 : 3);
    if (state.effect == "ammo")
    {
        m_reloadRemaining = 0.0f;
        if (!m_primary.type.empty())
        {
            const auto& profile = getWeaponProfile(m_primary.type);
            m_primary.ammo = profile.magazine_size;
            m_primary.reserve = profile.magazine_size * profile.reserve_magazines;
        }
        if (!m_secondary.type.empty())
        {
            const auto& profile = getWeaponProfile(m_secondary.type);
            m_secondary.ammo = profile.magazine_size;
            m_secondary.reserve = profile.magazine_size * profile.reserve_magazines;
        }
        if (m_activeSlot == 1 || m_activeSlot == 2)
        {
            m_ammo = (m_activeSlot == 1 ? m_primary : m_secondary).ammo;
            m_reserveAmmo = (m_activeSlot == 1 ? m_primary : m_secondary).reserve;
            refreshWeaponHud(app);
        }
    }
    if (m_activeSlot >= 4)
        refreshWeaponHud(app);
    if (state.effect == "health")
    {
        m_health = 100;
        app.ui->healthText.setString("HP: 100");
    }
}

void DeathmatchMode::onGrenadeThrown(ClientApp& app, const nx3d::economy::GrenadeThrow& event)
{
    if (event.owner == app.client_api.getClientId())
        return; // The thrower has already started the local animation.
    const glm::vec3 start(event.x, event.y, event.z);
    glm::vec3 direction(event.endX - event.x, 0.0f, event.endZ - event.z);
    if (glm::length(direction) < 0.001f)
        return;
    direction = glm::normalize(direction);
    showGrenade(app, start + direction * 18.0f,
                glm::vec3(event.endX, event.y, event.endZ), event.smoke);
}

void DeathmatchMode::handleKeyUp(ClientApp& app, const SDL_Event& event)
{
    if (event.key.key == SDLK_TAB)
        app.ui->hideLeaderboard();
}

void DeathmatchMode::equipGun(ClientApp& app,
                              const std::string& weaponName,
                              const std::string& objPath,
                              const std::string& mtlPath,
                              float fpScale,
                              glm::vec3 fpCenter,
                              glm::vec3 fpOffset,
                              glm::vec3 fpRotation)
{
    if (app.reload_audio)
        app.reload_audio->stop();
    if (m_crosshair)
    {
        vkDeviceWaitIdle(app.renderer.getDevice());
        m_crosshair.reset();
        app.ui->renderer().setOverlayCallback(nullptr);
    }
    if (m_firstPersonGun)
    {
        app.scene_manager.getCurrentScene()->removeObject(m_firstPersonGun->getId());
        vkDeviceWaitIdle(app.renderer.getDevice());
        m_firstPersonGun.reset();
    }

    m_firstPersonGun = std::make_shared<nx3d::client::gun::FirstPersonGun>(
            app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
            app.renderer.getCommandPool(), app.renderer.getGraphicsQueue(),
            objPath, mtlPath,
            app.descriptorPool, app.descriptorSetLayout,
            blt::ObjectData(app.shared_world, 0.0f,
                            glm::vec3(0.0f, -1000.0f, 0.0f), app.default_size),
            fpScale, fpCenter);
    m_firstPersonGun->setViewOffset(fpOffset);
    m_firstPersonGun->setViewRotation(fpRotation);
    app.scene_manager.getCurrentScene()->addObject(m_firstPersonGun);

    if (m_activeSlot <= 2)
        m_crosshair = std::make_shared<nx3d::client::Crosshair>(
                app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                app.crosshair_texture, app.descriptorPool, app.descriptorSetLayout,
                blt::ObjectData(app.shared_world, 0.0f,
                                glm::vec3(0.0f, -1000.0f, 0.0f), app.default_size));
    installOverlay(app);

    m_weaponName = weaponName;
    const auto& profile = getWeaponProfile(m_pickedUpItemType);
    m_magazineSize = profile.magazine_size;
    m_ammo = m_magazineSize;
    m_reserveAmmo = m_magazineSize * profile.reserve_magazines;
    m_reloadRemaining = 0.0f;
    refreshWeaponHud(app);
}

void DeathmatchMode::installOverlay(ClientApp& app)
{
    app.ui->renderer().setOverlayCallback([this, &app]()
                                          {
        auto ctx = spear::ui::RenderContext{spear::rendering::vulkan::g_frameContext.commandBuffer};
        if (app.currentState == ClientApp::State::Paused)
            app.ui->renderPauseOverlay(ctx, app.quitHovered,
                                       app.ctTeamHovered, app.tTeamHovered);
        else if (app.currentState == ClientApp::State::Buy)
            app.ui->renderBuyOverlay(ctx);
        else if (app.game_mode && app.game_mode->isEliminated())
            app.ui->renderDeathOverlay(ctx);
        if (app.ui->isLeaderboardVisible())
            app.ui->renderLeaderboardOverlay(ctx);
        app.ui->renderChatOverlay(ctx);
        if (m_crosshair && m_health > 0 && app.currentState == ClientApp::State::Game)
            m_crosshair->render(app.camera); });
}

glm::vec3 DeathmatchMode::pickSpawnPoint()
{
    const auto& spawns = (m_team == "Terrorist") ? nx3d::economy::kTSpawns : nx3d::economy::kCtSpawns;
    if (spawns.empty())
        return glm::vec3{0.0f};

    size_t index = rand() % spawns.size();
    if (spawns.size() > 1 && index == m_lastSpawnIndex)
        index = (index + 1) % spawns.size();
    m_lastSpawnIndex = index;
    return {spawns[index].x, spawns[index].y, spawns[index].z};
}

void DeathmatchMode::equipWeaponForTeam(ClientApp& app)
{
    if (app.ready && app.client_api.clientInRoom())
    {
        for (const auto& slot : {m_primary, m_secondary})
            if (slot.itemId != 0)
                app.tcp_client.sendMessage(nexilis::client::Packet::Room::GameItem::destroy(
                        app.client_api, slot.itemId));
    }
    m_primary = {};
    m_secondary = {};
    m_secondary.type = nx3d::economy::defaultWeapon(m_team);
    m_grenades = 0;
    m_smokes = 0;
    m_activeSlot = 0;
    equipSlot(app, 2);
}

void DeathmatchMode::equipSlot(ClientApp& app, int slot, bool force)
{
    if (slot < 1 || slot > 5 || m_health <= 0)
        return;
    GunSlot* selected = slot == 1 ? &m_primary : slot == 2 ? &m_secondary : nullptr;
    if ((selected && selected->type.empty()) || (slot == 4 && m_grenades == 0) ||
        (slot == 5 && m_smokes == 0))
        return;
    if (!force && slot == m_activeSlot &&
        m_pickedUpItemType == (selected ? selected->type : slot == 3 ? "knife" :
                              slot == 4 ? "grenade" : "smoke"))
        return;
    GunSlot* previous = m_activeSlot == 1 ? &m_primary :
                        m_activeSlot == 2 ? &m_secondary : nullptr;
    if (previous && previous != selected && previous->type == m_pickedUpItemType)
    {
        previous->ammo = m_ammo;
        previous->reserve = m_reserveAmmo;
        previous->itemId = m_pickedUpItemId;
        previous->itemSize = m_pickedUpItemSize;
    }
    m_activeSlot = slot;
    m_triggerHeld = false;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;
    m_pickedUpItemType = selected ? selected->type : slot == 3 ? "knife" :
                         slot == 4 ? "grenade" : "smoke";
    m_weaponPickedUp = slot <= 3;
    m_pickedUpItemId = selected ? selected->itemId : 0;
    m_pickedUpItemSize = selected ? selected->itemSize : glm::vec3(1.0f);
    const auto& profile = getWeaponProfile(m_pickedUpItemType);
    equipGun(app, profile.name, profile.obj_path, profile.mtl_path,
             profile.fp_scale, profile.fp_center, profile.fp_offset, profile.fp_rotation);
    if (selected && selected->ammo >= 0)
        m_ammo = selected->ammo;
    if (selected && selected->reserve >= 0)
        m_reserveAmmo = selected->reserve;
    if (slot >= 3)
    {
        m_ammo = 0;
        m_reserveAmmo = 0;
    }
    refreshWeaponHud(app);
}

void DeathmatchMode::throwGrenade(ClientApp& app)
{
    const bool smoke = m_activeSlot == 5;
    int& count = smoke ? m_smokes : m_grenades;
    if (!app.ready || !app.client_api.clientInRoom() || count <= 0)
        return;
    using packet = nexilis::client::Packet;
    const glm::vec3 origin = app.camera.getPosition();
    glm::vec3 direction = app.camera.getFront();
    direction.y = 0.0f;
    if (glm::length(direction) < 0.001f)
        direction = glm::vec3(0.0f, 0.0f, -1.0f);
    direction = glm::normalize(direction);
    const float yaw = std::atan2(-direction.x, -direction.z);
    app.tcp_client.sendMessage(packet::Room::Player3D::position(
            app.client_api, nexilis::Vector3f({origin.x, origin.y, origin.z}), yaw));
    app.tcp_client.sendMessage(packet::Room::Player3D::matchAction(
            app.client_api, smoke ? nx3d::economy::kThrowSmokeAction :
                                    nx3d::economy::kThrowGrenadeAction));
    --count;
    showGrenade(app, origin + direction * 18.0f,
                origin + direction * nx3d::economy::kGrenadeThrowDistance, smoke);
    if (count == 0)
        equipSlot(app, !m_primary.type.empty() ? 1 :
                      !m_secondary.type.empty() ? 2 : 3);
    else
        refreshWeaponHud(app);
}

void DeathmatchMode::showGrenade(ClientApp& app, glm::vec3 start, glm::vec3 end, bool smoke)
{
    auto model = std::make_shared<spear::rendering::vulkan::TexturedCube>(
            app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
            smoke ? app.smoke_texture : app.crosshair_texture,
            app.descriptorPool, app.descriptorSetLayout,
            blt::ObjectData(app.shared_world, 0.0f, start, app.default_size));
    model->scale(glm::vec3(8.0f));
    app.scene_manager.getCurrentScene()->addObject(model);
    m_grenadeVisuals.push_back({model, start, end, 0.0f, smoke, {}});
}

void DeathmatchMode::refreshWeaponHud(ClientApp& app)
{
    if (m_weaponName.empty())
        return;
    if (m_activeSlot == 3)
    {
        app.ui->weaponHudText.setString("3 Knife  |  1 Primary  2 Secondary  4 HE  5 Smoke");
        return;
    }
    if (m_activeSlot == 4)
    {
        app.ui->weaponHudText.setString("4 HE Grenade x" + std::to_string(m_grenades) +
                                        "  |  Left click: Throw");
        return;
    }
    if (m_activeSlot == 5)
    {
        app.ui->weaponHudText.setString("5 Smoke Grenade x" + std::to_string(m_smokes) +
                                        "  |  Left click: Throw");
        return;
    }
    const std::string suffix = m_reloadRemaining > 0.0f ? "  RELOADING" :
                               m_ammo == 0 && m_reserveAmmo > 0 ? "  [R] RELOAD" :
                               m_ammo == 0 ? "  OUT OF AMMO" : "";
    app.ui->weaponHudText.setString(std::to_string(m_activeSlot) + " " + m_weaponName + "  " +
                                    std::to_string(m_ammo) + "/" +
                                    std::to_string(m_reserveAmmo) + suffix);
}

void DeathmatchMode::startReload(ClientApp& app)
{
    if (m_reloadRemaining > 0.0f || m_ammo >= m_magazineSize || m_reserveAmmo <= 0)
        return;
    m_triggerHeld = false;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;
    m_reloadRemaining = getWeaponProfile(m_pickedUpItemType).reload_seconds;
    if (m_firstPersonGun)
        m_firstPersonGun->startReload(m_reloadRemaining);
    refreshWeaponHud(app);
    if (app.reload_audio)
        app.reload_audio->play();
    if (app.ready && app.client_api.clientInRoom())
    {
        const glm::vec3 pos = app.camera.getPosition();
        app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::audioEvent(
                app.client_api, audio::kSoundReload,
                nexilis::Vector3f({pos.x, pos.y, pos.z})));
    }
}

void DeathmatchMode::dropCurrentWeapon(ClientApp& app)
{
    using packet = nexilis::client::Packet;

    if (!app.ready || !app.client_api.clientInRoom())
        return;

    m_triggerHeld = false;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;

    // If we were holding a map pickup, remove the original from the server.
    if (m_pickedUpItemId != 0)
    {
        app.tcp_client.sendMessage(packet::Room::GameItem::destroy(app.client_api, m_pickedUpItemId));
        m_pickedUpItemId = 0;
    }

    // Place the pickup just ahead of the player. The local physics body gets
    // a forward impulse when the server echoes the newly created item.
    glm::vec3 throwDirection = app.camera.getFront();
    throwDirection.y = 0.0f;
    if (glm::length(throwDirection) < 0.001f)
        throwDirection = glm::vec3(0.0f, 0.0f, -1.0f);
    throwDirection = glm::normalize(throwDirection);
    glm::vec3 dropPos = app.camera.getPosition() + throwDirection * 25.0f;
    dropPos.y -= 10.0f;
    auto pos = nexilis::Vector3f({dropPos.x, dropPos.y, dropPos.z});
    auto dim = nexilis::Vector3f({m_pickedUpItemSize.x, m_pickedUpItemSize.y, m_pickedUpItemSize.z});
    const auto& profile = getWeaponProfile(m_pickedUpItemType);
    app.tcp_client.sendMessage(
            packet::Room::GameItem::create(app.client_api, pos, dim,
                                           nx3d::ammunition::encodePickup(profile.type, m_ammo,
                                                                           m_reserveAmmo),
                                           "on_ground", profile.obj_path));
    app.tcp_client.sendMessage(packet::Room::Player3D::matchAction(
            app.client_api, m_activeSlot == 1 ? nx3d::economy::kDropPrimaryAction :
                                            nx3d::economy::kDropSecondaryAction));
    app.pending_weapon_throw = ClientApp::PendingWeaponThrow{dropPos, throwDirection};
    (m_activeSlot == 1 ? m_primary : m_secondary) = {};
    equipSlot(app, m_activeSlot == 1 ? (!m_secondary.type.empty() ? 2 : 3) :
                  (!m_primary.type.empty() ? 1 : 3));
}

void DeathmatchMode::pickupNearbyWeapon(ClientApp& app)
{
    using packet = nexilis::client::Packet;
    if (!app.ready || !app.client_api.clientInRoom())
        return;

    const glm::vec3 camPos = app.camera.getPosition();
    const auto gameItems = app.client_api.getRemoteGameItemsSnapshot(app.client_api.clientRoomId());
    for (const auto& item : gameItems)
    {
        const std::string weaponType(nx3d::ammunition::pickupWeaponType(item.item_type));
        if (item.status != "on_ground" || !hasWeaponProfile(weaponType))
            continue;
        int magazine = 0, reserve = 0;
        const bool hasAmmo = nx3d::ammunition::decodePickup(item.item_type, magazine, reserve);
        if (item.item_type.find(':') != std::string::npos && !hasAmmo)
            continue;
        const int slot = nx3d::economy::isSecondary(weaponType) ? 2 : 1;
        GunSlot& inventory = slot == 1 ? m_primary : m_secondary;
        if (!inventory.type.empty())
            continue;
        glm::vec3 itemPos(item.x, item.y, item.z);
        auto localItem = app.remote_game_items.find(item.id);
        if (localItem != app.remote_game_items.end())
        {
            const btVector3 physicsPos = localItem->second->getPosition();
            itemPos = glm::vec3(physicsPos.x(), physicsPos.y(), physicsPos.z());
        }
        if (glm::distance(camPos, itemPos) >= 100.0f)
            continue;

        inventory.type = weaponType;
        inventory.ammo = -1;
        inventory.reserve = -1;
        if (hasAmmo)
        {
            const auto& profile = getWeaponProfile(weaponType);
            inventory.ammo = std::min(magazine, profile.magazine_size);
            inventory.reserve = std::min(reserve,
                                         profile.magazine_size * profile.reserve_magazines);
        }
        inventory.itemId = item.id;
        inventory.itemSize = glm::vec3(item.w, item.h, item.d);
        app.tcp_client.sendMessage(packet::Room::GameItem::update(app.client_api, item.id, "picked_up"));

        if (localItem != app.remote_game_items.end())
        {
            app.scene_manager.getCurrentScene()->removeObject(localItem->second->getId());
            app.pendingDestroy[(app.frameCount - 1) % 3].push_back(std::move(localItem->second));
            app.remote_game_items.erase(localItem);
        }

        equipSlot(app, slot);
        return;
    }
}

} // namespace nx3d::client::gamemode
