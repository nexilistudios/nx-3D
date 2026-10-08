#include <client/gamemode/bomb_mode.hh>

#include <client/app/client_app.hh>

#include <nexilis/client/packet.hh>

#include <algorithm>
#include <cmath>

namespace nx3d::client::gamemode
{
namespace blt = spear::physics::bullet;

void BombMode::onEnter(ClientApp& app, const std::string& team)
{
    DeathmatchMode::onEnter(app, team);
    app.ui->roundText.setString("WAITING FOR MATCH");
    app.ui->matchStatusText.setString("Queued for next round");
    app.ui->bombText.setString("");
}

void BombMode::update(ClientApp& app, float dt)
{
    if (!m_active || !m_alive || m_phase != "live")
        m_triggerHeld = false;
    DeathmatchMode::update(app, dt);

    for (const auto& event : app.client_api.consumeMatchEvents())
    {
        if (event.room_id != app.client_api.clientRoomId())
            continue;
        const bool newRound = event.round != m_round;
        const bool bombExploded = m_phase == "live" && event.phase == "post" &&
                                  event.notice == "T win: Bomb exploded";
        if (bombExploded && app.explosion_audio)
            app.explosion_audio->play();
        m_round = event.round;
        m_tScore = event.terrorist_score;
        m_ctScore = event.counter_terrorist_score;
        m_queuePosition = event.queue_position;
        m_phase = event.phase;
        m_notice = event.notice;
        m_seconds = event.seconds;
        m_planted = event.bomb_planted;
        m_active = event.active;
        m_alive = event.alive;
        if ((!m_alive || event.phase != "live") && m_using && app.planting_audio)
            app.planting_audio->stop();
        if (m_planted)
        {
            if (!m_bombCube)
            {
                m_bombCube = std::make_shared<spear::rendering::vulkan::TexturedCube>(
                        app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                        app.crosshair_texture, app.descriptorPool, app.descriptorSetLayout,
                        blt::ObjectData(app.shared_world, 0.0f,
                                        glm::vec3(event.bomb_x, event.bomb_y, event.bomb_z),
                                        app.default_size),
                        glm::vec4(1.0f, 0.15f, 0.05f, 1.0f));
                m_bombCube->scale(glm::vec3(20.0f));
                app.scene_manager.getCurrentScene()->addObject(m_bombCube);
            }
            m_bombCube->setPosition({event.bomb_x, event.bomb_y, event.bomb_z});
        }
        else if (m_bombCube)
        {
            app.scene_manager.getCurrentScene()->removeObject(m_bombCube->getId());
            app.pendingDestroy[(app.frameCount - 1) % 3].push_back(std::move(m_bombCube));
        }
        if (!m_alive)
            m_triggerHeld = false;
        const bool teamChanged = !event.team.empty() && m_team != event.team;
        if (teamChanged)
        {
            m_team = event.team;
            app.ui->teamDisplayText.setString(m_team);
        }
        if (newRound && event.active && event.phase == "live")
        {
            if (teamChanged)
            {
                m_lastSpawnIndex = SIZE_MAX;
                app.camera.setPosition(pickSpawnPoint());
                m_prevCamPos = app.camera.getPosition();
            }
            m_weaponPickedUp = true;
            equipWeaponForTeam(app);
        }
    }

    if (m_phase == "live" || m_phase == "post")
        m_seconds = std::max(0.0f, m_seconds - dt);
    const int seconds = static_cast<int>(std::ceil(m_seconds));
    const std::string roundText = "T " + std::to_string(m_tScore) + " : " +
                                  std::to_string(m_ctScore) + " CT   ROUND " + std::to_string(m_round) +
                                  "/24   " + std::to_string(seconds) + "s";
    if (app.ui->roundText.getString() != roundText)
        app.ui->roundText.setString(roundText);
    std::string status = m_notice;
    if (m_queuePosition)
        status = "Queue #" + std::to_string(m_queuePosition) + " | Next round";
    else if (m_phase == "live" && !m_alive)
        status = "Eliminated | Waiting for next round";
    else if (m_phase == "live" && m_alive)
        status = m_planted ? (m_team == "Counter Terrorist" ? "Hold E near bomb to defuse" : "Defend the bomb")
                           : (m_team == "Terrorist" ? "Hold E to plant bomb" : "Stop the plant");
    if (app.ui->matchStatusText.getString() != status)
        app.ui->matchStatusText.setString(status);
    const std::string bombText = m_planted ? "[■] BOMB" : "";
    if (app.ui->bombText.getString() != bombText)
        app.ui->bombText.setString(bombText);
}

void BombMode::handleMouseButtonDown(ClientApp& app, const SDL_Event& event)
{
    if (m_active && m_alive && m_phase == "live")
        DeathmatchMode::handleMouseButtonDown(app, event);
}

void BombMode::handleKeyDown(ClientApp& app, const SDL_Event& event)
{
    if (event.key.key == SDLK_E && !event.key.repeat && m_active && m_alive && m_phase == "live")
    {
        const uint8_t action = m_planted ? 2 : 1;
        if ((action == 1 && m_team == "Terrorist") ||
            (action == 2 && m_team == "Counter Terrorist"))
        {
            app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::matchAction(app.client_api, action));
            m_using = true;
            if (action == 1)
            {
                if (app.planting_audio)
                    app.planting_audio->play();
                const glm::vec3 pos = app.camera.getPosition();
                app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::audioEvent(
                        app.client_api, audio::kSoundPlanting,
                        nexilis::Vector3f({pos.x, pos.y, pos.z})));
            }
        }
    }
    if (m_active && m_alive)
        DeathmatchMode::handleKeyDown(app, event);
    else if (event.key.key == SDLK_TAB)
        DeathmatchMode::handleKeyDown(app, event);
}

void BombMode::handleKeyUp(ClientApp& app, const SDL_Event& event)
{
    if (event.key.key == SDLK_E && m_using)
    {
        app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::matchAction(app.client_api, 0));
        if (app.planting_audio)
            app.planting_audio->stop();
        m_using = false;
    }
    DeathmatchMode::handleKeyUp(app, event);
}
} // namespace nx3d::client::gamemode
