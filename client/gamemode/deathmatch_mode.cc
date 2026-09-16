#include <client/app/client_app.hh>
#include <client/gamemode/deathmatch_mode.hh>
#include <client/gamemode/weapon_profiles.hh>

#include <nexilis/client/packet.hh>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace nx3d::client::gamemode
{

namespace blt = spear::physics::bullet;

nx3d::GameMode DeathmatchMode::type() const
{
    return nx3d::GameMode::deathmatch;
}

void DeathmatchMode::onEnter(ClientApp& app, const std::string& team)
{
    using packet = nexilis::client::Packet;

    m_team = team;
    app.ui->teamDisplayText.setString(m_team);

    // Tell the server which team this player joined so it can track kills and
    // deaths per team. The server replies with a stats seed that the client
    // merges into its locally-maintained leaderboard.
    app.tcp_client.sendMessage(packet::Room::Player3D::setTeam(app.client_api, team));
    app.ui->hideLeaderboard();
    m_leaderboard.clear();

    const auto& spawns = (m_team == "Terrorist") ? m_tSpawns : m_ctSpawns;
    m_lastSpawnIndex = SIZE_MAX;
    if (!spawns.empty())
        app.camera.setPosition(pickSpawnPoint());

    m_health = 100;
    app.ui->healthText.setString("HP: " + std::to_string(m_health));
    m_prevCamPos = app.camera.getPosition();

    m_weaponPickedUp = true;
    equipWeaponForTeam(app);
}

void DeathmatchMode::update(ClientApp& app, float delta_time)
{
    using packet = nexilis::client::Packet;

    if (m_dropCooldown > 0)
        m_dropCooldown--;
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
    updateWalkSound(app, std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z));

    if (m_firstPersonGun)
        m_firstPersonGun->addBob(delta_time, velocity);

    // --- Pickup nearby dropped weapons ---
    if (!m_weaponPickedUp && m_dropCooldown == 0 && app.ready && app.client_api.clientInRoom())
    {
        glm::vec3 camPos = app.camera.getPosition();
        auto game_items = app.client_api.getRemoteGameItemsSnapshot(app.client_api.clientRoomId());

        for (auto& item : game_items)
        {
            if (item.status == "on_ground")
            {
                float dist = glm::distance(camPos, glm::vec3(item.x, item.y, item.z));
                if (dist < 100.0f)
                {
                    m_weaponPickedUp = true;
                    m_pickedUpItemId = item.id;
                    m_pickedUpItemSize = glm::vec3(item.w, item.h, item.d);
                    m_pickedUpItemType = item.item_type;
                    m_pickedUpItemFilepath = item.filepath;

                    app.tcp_client.sendMessage(
                            packet::Room::GameItem::update(app.client_api, item.id, "picked_up"));

                    if (app.remote_game_items.find(item.id) != app.remote_game_items.end())
                    {
                        app.scene_manager.getCurrentScene()->removeObject(app.remote_game_items[item.id]->getId());
                        app.pendingDestroy[(app.frameCount - 1) % 3].push_back(
                                std::move(app.remote_game_items[item.id]));
                        app.remote_game_items.erase(item.id);
                    }

                    const auto& profile = getWeaponProfile(item.item_type);
                    equipGun(app, profile.name, profile.obj_path, profile.mtl_path,
                             profile.fp_scale, profile.fp_center);

                    break;
                }
            }
        }
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
        }
        else if (evt.damage > 0.0f)
        {
            m_hitmarkerFrames = 15;
        }
    }

    // --- Consume respawn events ---
    auto respawnEvents = app.client_api.consumeRespawnEvents();
    for (auto& evt : respawnEvents)
    {
        if (evt.target_id == app.client_api.getClientId())
        {
            const auto& spawns = (m_team == "Terrorist") ? m_tSpawns : m_ctSpawns;
            if (!spawns.empty())
                app.camera.setPosition(pickSpawnPoint());
            m_health = 100;
            app.ui->healthText.setString("HP: " + std::to_string(m_health));

            if (!m_weaponPickedUp)
            {
                m_weaponPickedUp = true;
                equipWeaponForTeam(app);
            }
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
    using packet = nexilis::client::Packet;

    if (!m_weaponPickedUp || event.button.button != SDL_BUTTON_LEFT)
        return;

    if (app.gunshot_audio)
        app.gunshot_audio->play();

    // Capture the aim direction BEFORE the aim punch is applied, so the
    // bullet goes exactly where the crosshair points when firing. The punch
    // below is purely visual feedback and decays after the shot.
    glm::vec3 rayOrigin = app.camera.getPosition();
    glm::vec3 rayDir = glm::normalize(app.camera.getFront());

    if (m_firstPersonGun)
        m_firstPersonGun->addRecoil(0.1f);

    // CS-style aim punch: kick the view up with a little random yaw,
    // decaying over time.
    float yawKick = (static_cast<float>(rand() % 100) - 50.0f) / 50.0f * 0.6f;
    app.camera.addRecoilOffset(1.4f, yawKick);

    auto room_id = app.client_api.clientRoomId();
    auto my_id = app.client_api.getClientId();
    auto players = app.client_api.getRemotePlayersSnapshot(room_id, my_id);

    float closestDist = std::numeric_limits<float>::max();
    uint64_t hitTargetId = 0;

    for (auto& player : players)
    {
        glm::vec3 center(player.x, player.y, player.z);
        glm::vec3 halfExtents(10.0f, 10.0f, 10.0f);
        glm::vec3 boxMin = center - halfExtents;
        glm::vec3 boxMax = center + halfExtents;

        float tmin = -std::numeric_limits<float>::max();
        float tmax = std::numeric_limits<float>::max();
        bool miss = false;

        for (int i = 0; i < 3; i++)
        {
            float origin_i = (&rayOrigin.x)[i];
            float dir_i = (&rayDir.x)[i];
            float min_i = (&boxMin.x)[i];
            float max_i = (&boxMax.x)[i];

            if (std::abs(dir_i) < 1e-8f)
            {
                if (origin_i < min_i || origin_i > max_i)
                {
                    miss = true;
                    break;
                }
            }
            else
            {
                float invD = 1.0f / dir_i;
                float t1 = (min_i - origin_i) * invD;
                float t2 = (max_i - origin_i) * invD;
                if (t1 > t2)
                    std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax)
                {
                    miss = true;
                    break;
                }
            }
        }

        if (!miss && tmin >= 0.0f && tmin < closestDist)
        {
            closestDist = tmin;
            hitTargetId = player.id;
        }
    }

    if (hitTargetId != 0)
    {
        // Fire the hitmarker sound a little later (in update()) so it is
        // not drowned out by the gunshot sound that was just started.
        triggerHitmarkerSound();

        app.tcp_client.sendMessage(
                packet::Room::Player3D::shoot(app.client_api, hitTargetId, 35.0f));
    }
}

void DeathmatchMode::handleKeyDown(ClientApp& app, const SDL_Event& event)
{
    // Ignore OS auto-repeat: holding Tab/G must only trigger the action once.
    if (event.key.repeat)
        return;

    if (event.key.key == SDLK_G && m_weaponPickedUp)
        dropCurrentWeapon(app);
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
                              glm::vec3 fpCenter)
{
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
    app.scene_manager.getCurrentScene()->addObject(m_firstPersonGun);

    m_crosshair = std::make_shared<nx3d::client::Crosshair>(
            app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
            app.crosshair_texture, app.descriptorPool, app.descriptorSetLayout,
            blt::ObjectData(app.shared_world, 0.0f,
                            glm::vec3(0.0f, -1000.0f, 0.0f), app.default_size));
    app.ui->renderer().setOverlayCallback([this, &app]()
                                          {
        auto ctx = spear::ui::RenderContext{spear::rendering::vulkan::g_frameContext.commandBuffer};
        if (app.currentState == ClientApp::State::Paused)
            app.ui->renderPauseOverlay(ctx, app.quitHovered);
        if (app.ui->isLeaderboardVisible())
            app.ui->renderLeaderboardOverlay(ctx);
        if (m_crosshair)
            m_crosshair->render(app.camera);
    });

    app.ui->weaponHudText.setString(weaponName);
}

void DeathmatchMode::unequipGun(ClientApp& app)
{
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
    app.ui->weaponHudText.setString("");
}

glm::vec3 DeathmatchMode::pickSpawnPoint()
{
    const auto& spawns = (m_team == "Terrorist") ? m_tSpawns : m_ctSpawns;
    if (spawns.empty())
        return glm::vec3{0.0f};

    size_t index = rand() % spawns.size();
    if (spawns.size() > 1 && index == m_lastSpawnIndex)
        index = (index + 1) % spawns.size();
    m_lastSpawnIndex = index;
    return spawns[index];
}

void DeathmatchMode::equipWeaponForTeam(ClientApp& app)
{
    const bool isCt = (m_team == "Counter Terrorist");
    const auto& profile = getWeaponProfile(isCt ? "m4" : "ak47");

    equipGun(app, profile.name, profile.obj_path, profile.mtl_path,
             profile.fp_scale, profile.fp_center);

    m_pickedUpItemId = 0;
    m_pickedUpItemSize = glm::vec3(1.0f, 1.0f, 1.0f);
    m_pickedUpItemType = profile.type;
    m_pickedUpItemFilepath = profile.obj_path;
}

void DeathmatchMode::dropCurrentWeapon(ClientApp& app)
{
    using packet = nexilis::client::Packet;

    m_weaponPickedUp = false;
    unequipGun(app);

    // If we were holding a map pickup, remove the original from the server.
    if (m_pickedUpItemId != 0)
    {
        app.tcp_client.sendMessage(packet::Room::GameItem::destroy(app.client_api, m_pickedUpItemId));
        m_pickedUpItemId = 0;
    }

    // Always drop the held gun into the map, so even the team-issued
    // spawn gun ends up as a physics-driven pickup on the ground.
    glm::vec3 dropPos = app.camera.getPosition() + app.camera.getFront() * 80.0f;
    auto pos = nexilis::Vector3f({dropPos.x, dropPos.y, dropPos.z});
    auto dim = nexilis::Vector3f({m_pickedUpItemSize.x, m_pickedUpItemSize.y, m_pickedUpItemSize.z});
    app.tcp_client.sendMessage(
            packet::Room::GameItem::create(app.client_api, pos, dim,
                                           m_pickedUpItemType, "on_ground", m_pickedUpItemFilepath));

    m_dropCooldown = 60;
}

} // namespace nx3d::client::gamemode