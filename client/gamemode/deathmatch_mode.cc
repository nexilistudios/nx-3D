#include <client/app/client_app.hh>
#include <client/gamemode/deathmatch_mode.hh>
#include <client/gamemode/weapon_profiles.hh>
#include <shared/ammunition.hh>

#include <nexilis/client/packet.hh>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string_view>

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
        app.movement_controller.resetStance();
        app.movement_controller.resetVerticalMotion();
    }

    // The server replies with a stats seed for the local leaderboard.
    app.tcp_client.sendMessage(packet::Room::Player3D::setTeam(app.client_api, team));

    m_health = 100;
    m_hurtSoundCooldown = 0.0f;
    app.ui->healthText.setString("HP: " + std::to_string(m_health));
    app.ui->wastedText.setString("");
    m_prevCamPos = app.camera.getPosition();

    m_weaponPickedUp = true;
    m_objects = {};
    equipWeaponForTeam(app);
}

void DeathmatchMode::update(ClientApp& app, float delta_time)
{
    using packet = nexilis::client::Packet;

    m_fireCooldown = std::max(0.0f, m_fireCooldown - delta_time);
    m_hurtSoundCooldown = std::max(0.0f, m_hurtSoundCooldown - delta_time);
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
                    if (app.graphicsDetail == 0 && puff % 2 != 0) continue;
                    if (app.graphicsDetail == 1 && puff >= 8 && puff % 2 != 0) continue;
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
                    it->puffSlots.push_back(puff);
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
                const int slot = it->puffSlots[puff];
                const float angle = static_cast<float>(slot % 8) * 0.78539816f;
                const float radius = slot < 8 ? 34.0f : 59.0f;
                const float height = slot < 8 ? -13.0f : 16.0f;
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
            if (evt.damage > 0.0f && m_hurtSoundCooldown <= 0.0f)
            {
                app.playVoiceCue(audio::VoiceCue::hurt);
                m_hurtSoundCooldown = 0.35f;
            }
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
                app.movement_controller.resetStance();
                app.movement_controller.resetVerticalMotion();
            }
            m_prevCamPos = app.camera.getPosition();
            m_triggerHeld = false;
            m_burstShots = 0;
            m_fireCooldown = 0.0f;
            if (m_firstPersonGun)
                m_firstPersonGun->resetAnimation();
            m_health = 100;
            m_hurtSoundCooldown = 0.0f;
            app.ui->healthText.setString("HP: " + std::to_string(m_health));
            app.ui->wastedText.setString("");
            m_reloadRemaining = 0.0f;
            if (app.reload_audio)
                app.reload_audio->stop();
            refreshWeaponHud(app);

            if (!preserveEquipmentOnRespawn())
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
        (m_pickedUpItemType == "grenade" || m_pickedUpItemType == "smoke"))
    {
        throwGrenade(app);
        return;
    }
    if (m_health <= 0 || !m_weaponPickedUp || event.button.button != SDL_BUTTON_LEFT)
        return;
    if (m_reloadRemaining > 0.0f)
        return;
    if (m_activeSlot <= 2 && m_ammo == 0)
    {
        if (app.empty_trigger_audio)
            app.empty_trigger_audio->play();
        return;
    }
    if (m_fireCooldown > 0.0f)
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
        const glm::vec3 direction = glm::normalize(app.camera.getFront());
        app.tcp_client.sendMessage(packet::Room::Player3D::fire(
                app.client_api, nexilis::Vector3f(direction.x, direction.y, direction.z), 3));
        return;
    }

    if (m_reloadRemaining > 0.0f || m_ammo == 0)
    {
        if (m_ammo == 0 && app.empty_trigger_audio)
            app.empty_trigger_audio->play();
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
    glm::vec3 rayDir = glm::normalize(app.camera.getFront());

    if (m_firstPersonGun)
        m_firstPersonGun->addRecoil(0.1f * recoilMul);

    // CS-style aim punch: kick the view up with a little random yaw,
    // decaying over time. Scales up with burst length.
    float yawKick = (static_cast<float>(rand() % 100) - 50.0f) / 50.0f * 0.6f * recoilMul;
    app.camera.addRecoilOffset(1.4f * recoilMul, yawKick);

    app.tcp_client.sendMessage(packet::Room::Player3D::fire(
            app.client_api, nexilis::Vector3f(rayDir.x, rayDir.y, rayDir.z),
            static_cast<uint8_t>(m_activeSlot)));
}

void DeathmatchMode::handleKeyDown(ClientApp& app, const SDL_Event& event)
{
    // Ignore OS auto-repeat: holding Tab/G must only trigger the action once.
    if (event.key.repeat)
        return;

    if (m_health > 0 && event.key.key >= SDLK_1 && event.key.key <= SDLK_3)
        equipSlot(app, static_cast<int>(event.key.key - SDLK_0));
    else if (m_health > 0 && event.key.key == SDLK_4 && m_objects.count("bomb") > 0)
        equipSlot(app, nx3d::findNXObject("bomb")->slot);
    else if (m_health > 0 && event.key.key == SDLK_4 && utilityKey() == 4)
    {
        if (const int slot = nextUtilitySlot()) equipSlot(app, slot);
    }
    else if (m_health > 0 && event.key.key == SDLK_5 && utilityKey() == 5)
    {
        if (const int slot = nextUtilitySlot()) equipSlot(app, slot);
    }
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
    m_objects = {};
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
    m_objects = state.objects;
    if ((state.effect == "bomb_spawn" || state.effect == "bomb_pickup") &&
        m_objects.count("bomb") > 0)
        equipSlot(app, nx3d::findNXObject("bomb")->slot);
    else if (state.effect == "weapon" && hasWeaponProfile(state.weapon))
    {
        const int slot = nx3d::economy::isSecondary(state.weapon) ? 2 : 1;
        GunSlot& purchased = slot == 1 ? m_primary : m_secondary;
        purchased.type = state.weapon;
        purchased.ammo = -1;
        purchased.reserve = -1;
        purchased.itemId = 0;
        purchased.itemSize = glm::vec3(1.0f);
        equipSlot(app, slot, true);
    }
    else if ((m_activeSlot == 1 && m_primary.type.empty()) ||
             (m_activeSlot == 2 && m_secondary.type.empty()) ||
             (m_activeSlot == 4 && m_objects.count("grenade") == 0) ||
             (m_activeSlot == 5 && m_objects.count("smoke") == 0) ||
             (m_activeSlot == 6 && m_objects.count("bomb") == 0) ||
             (m_activeSlot == 7 && m_objects.count("defuse_kit") == 0))
    {
        const int utility = nextUtilitySlot();
        equipSlot(app, utility ? utility : !m_primary.type.empty() ? 1 :
                      !m_secondary.type.empty() ? 2 : 3);
    }
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
                glm::vec3(event.endX, event.y, event.endZ),
                event.objectId == "smoke");
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
            fpScale, fpCenter, hasWeaponProfile(m_pickedUpItemType));
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
                                       app.ctTeamHovered, app.tTeamHovered,
                                       app.settingsHovered);
        else if (app.currentState == ClientApp::State::Settings)
            app.ui->renderSettingsOverlay(ctx, app.settingsRow);
        else if (app.currentState == ClientApp::State::Buy)
            app.ui->renderBuyOverlay(ctx);
        else if (app.game_mode && app.game_mode->isEliminated())
            app.ui->renderDeathOverlay(ctx);
        if (app.ui->isLeaderboardVisible())
            app.ui->renderLeaderboardOverlay(ctx);
        app.ui->renderChatOverlay(ctx);
        if (app.currentState == ClientApp::State::Game ||
            app.currentState == ClientApp::State::Paused)
            app.ui->renderPlayerNameplates(ctx);
        if (app.currentState == ClientApp::State::Game && m_crosshair && m_health > 0)
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
    // Keep the latest server-issued NXObjects. Bomb round snapshots can arrive
    // before the round event that calls this function.
    m_primary = {};
    m_secondary = {};
    m_secondary.type = nx3d::economy::defaultWeapon(m_team);
    m_activeSlot = 0;
    equipSlot(app, m_objects.count("bomb") > 0 ? nx3d::findNXObject("bomb")->slot : 2);
}

int DeathmatchMode::utilityKey() const
{
    return m_objects.count("bomb") > 0 ? 5 : 4;
}

int DeathmatchMode::nextUtilitySlot() const
{
    constexpr std::array<std::string_view, 3> utility{"grenade", "smoke", "defuse_kit"};
    int start = 0;
    for (int i = 0; i < static_cast<int>(utility.size()); ++i)
        if (m_activeSlot == nx3d::findNXObject(utility[i])->slot)
            start = i + 1;
    for (int offset = 0; offset < static_cast<int>(utility.size()); ++offset)
    {
        const auto* object = nx3d::findNXObject(utility[(start + offset) % utility.size()]);
        if (m_objects.count(object->id) > 0)
            return object->slot;
    }
    return 0;
}

void DeathmatchMode::equipSlot(ClientApp& app, int slot, bool force)
{
    if (slot < 1 || slot > 7 || m_health <= 0)
        return;
    GunSlot* selected = slot == 1 ? &m_primary : slot == 2 ? &m_secondary : nullptr;
    if ((selected && selected->type.empty()) ||
        (slot == 4 && m_objects.count("grenade") == 0) ||
        (slot == 5 && m_objects.count("smoke") == 0) ||
        (slot == 6 && m_objects.count("bomb") == 0) ||
        (slot == 7 && m_objects.count("defuse_kit") == 0))
        return;
    if (!force && slot == m_activeSlot &&
        m_pickedUpItemType == (selected ? selected->type : slot == 3 ? "knife" :
                              slot == 4 ? "grenade" : slot == 5 ? "smoke" :
                              slot == 6 ? "bomb" : "defuse_kit"))
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
                         slot == 4 ? "grenade" : slot == 5 ? "smoke" :
                         slot == 6 ? "bomb" : "defuse_kit";
    if (app.ready && app.client_api.clientInRoom())
        app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::matchAction(
                app.client_api, static_cast<uint8_t>(nx3d::economy::kEquipPrimaryAction + slot - 1)));
    m_weaponPickedUp = slot <= 3;
    m_pickedUpItemId = selected ? selected->itemId : 0;
    m_pickedUpItemSize = selected ? selected->itemSize : glm::vec3(1.0f);
    const auto& profile = getWeaponProfile(m_pickedUpItemType);
    equipGun(app, profile.name, profile.view_obj_path, profile.view_mtl_path,
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
    const bool smoke = m_pickedUpItemType == "smoke";
    const auto objectId = smoke ? "smoke" : "grenade";
    if (!app.ready || !app.client_api.clientInRoom() ||
        m_objects.count(objectId) == 0)
        return;
    using packet = nexilis::client::Packet;
    const glm::vec3 origin = app.camera.getPosition();
    glm::vec3 direction = app.camera.getFront();
    direction.y = 0.0f;
    if (glm::length(direction) < 0.001f)
        direction = glm::vec3(0.0f, 0.0f, -1.0f);
    direction = glm::normalize(direction);
    app.tcp_client.sendMessage(packet::Room::Player3D::matchAction(
            app.client_api, smoke ? nx3d::economy::kThrowSmokeAction :
                                    nx3d::economy::kThrowGrenadeAction));
    m_objects.consume(objectId);
    showGrenade(app, origin + direction * 18.0f,
                origin + direction * nx3d::economy::kGrenadeThrowDistance, smoke);
    if (m_objects.count(objectId) == 0)
    {
        const int utility = nextUtilitySlot();
        equipSlot(app, utility ? utility : !m_primary.type.empty() ? 1 :
                      !m_secondary.type.empty() ? 2 : 3);
    }
    else
        refreshWeaponHud(app);
}

void DeathmatchMode::showGrenade(ClientApp& app, glm::vec3 start, glm::vec3 end, bool smoke)
{
    const auto profile = getWeaponProfile(smoke ? "smoke" : "grenade");
    auto model = std::make_shared<spear::rendering::vulkan::OBJModel>(
            app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
            app.renderer.getCommandPool(), app.renderer.getGraphicsQueue(),
            profile.obj_path, profile.mtl_path,
            app.descriptorPool, app.descriptorSetLayout,
            blt::ObjectData(app.shared_world, 0.0f, start, app.default_size));
    model->scale(glm::vec3(profile.ground_scale));
    app.scene_manager.getCurrentScene()->addObject(model);
    m_grenadeVisuals.push_back({model, start, end, 0.0f, smoke, {}});
}

void DeathmatchMode::refreshWeaponHud(ClientApp& app)
{
    if (m_weaponName.empty())
        return;
    if (m_activeSlot == 3)
    {
        app.ui->weaponHudText.setString("3 Knife  |  1 Primary  2 Secondary  " +
                                        std::to_string(utilityKey()) + " Utility" +
                                        (m_objects.count("bomb") > 0 ? "  4 Bomb" : ""));
        return;
    }
    if (m_activeSlot == 4)
    {
        app.ui->weaponHudText.setString(std::to_string(utilityKey()) + " HE Grenade x" + std::to_string(m_objects.count("grenade")) +
                                        "  |  Left click: Throw");
        return;
    }
    if (m_activeSlot == 5)
    {
        app.ui->weaponHudText.setString(std::to_string(utilityKey()) + " Smoke Grenade x" + std::to_string(m_objects.count("smoke")) +
                                        "  |  Left click: Throw");
        return;
    }
    if (m_activeSlot == 6)
    {
        app.ui->weaponHudText.setString("4 C4 Bomb  |  Hold E at a bomb site to plant  |  G: Drop");
        return;
    }
    if (m_activeSlot == 7)
    {
        app.ui->weaponHudText.setString(std::to_string(utilityKey()) + " Defuse kit  |  Hold E near the bomb to defuse");
        return;
    }
    const std::string suffix = m_reloadRemaining > 0.0f ? "  RELOADING" :
                               m_ammo == 0 && m_reserveAmmo > 0 ? "  [R] RELOAD" :
                               m_ammo == 0 ? "  OUT OF AMMO" : "";
    app.ui->weaponHudText.setString(std::to_string(m_activeSlot) + " " + m_weaponName + "  " +
                                    std::to_string(m_ammo) + "/" +
                                    std::to_string(m_reserveAmmo) + suffix);
}

void DeathmatchMode::onAuthoritativeAnimation(ClientApp& app,
                                               const std::string& state,
                                               const std::string& weapon,
                                               float duration)
{
    if (state != "reloading" || weapon != m_pickedUpItemType ||
        m_activeSlot > 2 || m_health <= 0 || duration <= 0.0f)
        return;
    m_reloadRemaining = duration;
    if (m_firstPersonGun)
        m_firstPersonGun->startReload(duration);
    refreshWeaponHud(app);
}

void DeathmatchMode::startReload(ClientApp& app)
{
    if (m_reloadRemaining > 0.0f || m_ammo >= m_magazineSize || m_reserveAmmo <= 0)
        return;
    m_triggerHeld = false;
    m_burstShots = 0;
    m_fireCooldown = 0.0f;
    m_reloadRemaining = getWeaponProfile(m_pickedUpItemType).reload_seconds;
    if (app.ready && app.client_api.clientInRoom())
        app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::reload(
                app.client_api, static_cast<uint8_t>(m_activeSlot)));
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

    // The server creates the pickup from its own inventory and position.
    app.tcp_client.sendMessage(packet::Room::Player3D::matchAction(
            app.client_api, m_activeSlot == 1 ? nx3d::economy::kDropPrimaryAction :
                                            nx3d::economy::kDropSecondaryAction));
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
        const bool bomb = weaponType == "bomb";
        if (item.status != "on_ground" ||
            (!hasWeaponProfile(weaponType) && !bomb) ||
            (bomb && (m_team != "Terrorist" || m_objects.count("bomb") > 0)))
            continue;
        int magazine = 0, reserve = 0;
        const bool hasAmmo = !bomb && nx3d::ammunition::decodePickup(item.item_type, magazine, reserve);
        if (!bomb && item.item_type.find(':') != std::string::npos && !hasAmmo)
            continue;
        const int slot = bomb ? nx3d::findNXObject("bomb")->slot :
                         nx3d::economy::isSecondary(weaponType) ? 2 : 1;
        GunSlot* inventory = bomb ? nullptr : slot == 1 ? &m_primary : &m_secondary;
        if (inventory && !inventory->type.empty())
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

        if (inventory)
        {
            inventory->type = weaponType;
            inventory->ammo = -1;
            inventory->reserve = -1;
            if (hasAmmo)
            {
                const auto& profile = getWeaponProfile(weaponType);
                inventory->ammo = std::min(magazine, profile.magazine_size);
                inventory->reserve = std::min(reserve,
                                             profile.magazine_size * profile.reserve_magazines);
            }
            inventory->itemId = item.id;
            inventory->itemSize = glm::vec3(item.w, item.h, item.d);
        }
        app.tcp_client.sendMessage(packet::Room::GameItem::update(app.client_api, item.id, "picked_up"));

        if (localItem != app.remote_game_items.end())
        {
            app.scene_manager.getCurrentScene()->removeObject(localItem->second->getId());
            app.pendingDestroy[(app.frameCount - 1) % 3].push_back(std::move(localItem->second));
            app.remote_game_items.erase(localItem);
        }

        if (inventory)
            equipSlot(app, slot);
        return;
    }
}

} // namespace nx3d::client::gamemode
