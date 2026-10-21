#include <client/gamemode/bomb_mode.hh>

#include <client/app/client_app.hh>
#include <client/gamemode/bomb_player_state.hh>
#include <shared/bomb_sites.hh>
#include <shared/player_rules.hh>
#include <client/project_root.hh>
#include <shared/nx_objects.hh>

#include <nexilis/client/packet.hh>

#include <algorithm>
#include <cmath>

namespace nx3d::client::gamemode
{
namespace blt = spear::physics::bullet;
namespace
{
float standingEyeY(const ClientApp& app)
{
    return app.camera.getPosition().y +
           (app.movement_controller.isCrouched()
                    ? nx3d::kStandingEyeHeight - nx3d::kCrouchingEyeHeight
                    : 0.0f);
}
} // namespace

bool BombMode::canMove() const
{
    return !isEliminated();
}

bool BombMode::canBuy() const
{
    return m_phase == "live" && m_active && m_alive && DeathmatchMode::canMove();
}

bool BombMode::isEliminated() const
{
    return isBombPlayerEliminated(m_round, m_active, m_alive,
                                  DeathmatchMode::isEliminated());
}

bool BombMode::preserveEquipmentOnRespawn() const
{
    return (m_round > 0 && m_round != 12 && m_phase == "post" && m_active && m_alive) ||
           (m_phase == "live" && m_preservedRound == m_round);
}

void BombMode::onEnter(ClientApp& app, const std::string& team)
{
    DeathmatchMode::onEnter(app, team);
    m_using = false;
    m_defusing = false;
    m_preservedRound = 0;
    m_defuseSoundTimer = 0.0f;
    if (app.defusing_audio)
        app.defusing_audio->stop();
    app.ui->roundText.setString("WAITING FOR MATCH");
    app.ui->matchStatusText.setString("Queued for next round");
    app.ui->bombText.setString("");
}

void BombMode::update(ClientApp& app, float dt)
{
    if (!m_active || !m_alive || (m_phase != "live" && m_phase != "post"))
        m_triggerHeld = false;
    DeathmatchMode::update(app, dt);

    for (const auto& matchEvent : app.client_api.consumeMatchEvents())
    {
        if (!matchEvent || matchEvent->kind() != nexilis::client::ClientAPI::MatchEvent::Kind::bomb ||
            matchEvent->room_id != app.client_api.clientRoomId())
            continue;
        const auto& event = static_cast<const nexilis::client::ClientAPI::BombMatchEvent&>(*matchEvent);
        const bool newRound = event.round != m_round;
        const bool survivedPreviousRound = m_round > 0 && m_round != 12 &&
                                           m_phase == "post" && m_active && m_alive;
        const bool bombExploded = m_phase == "live" && event.phase == "post" &&
                                  event.notice == "T win: Bomb exploded";
        if (bombExploded && app.explosion_audio)
            app.explosion_audio->play();
        if (m_phase == "live" && event.phase == "live" && !m_planted && event.bomb_planted)
            app.playVoiceCue(audio::VoiceCue::bombPlanted);
        if (m_phase == "live" && event.phase == "post")
        {
            if (event.notice == "CT win: Bomb defused")
                app.playVoiceCue(audio::VoiceCue::bombDefused);
            else if (event.notice.starts_with("T win: "))
                app.playVoiceCue(audio::VoiceCue::terroristsWin);
            else if (event.notice.starts_with("CT win: "))
                app.playVoiceCue(audio::VoiceCue::counterTerroristsWin);
        }
        m_round = event.round;
        m_tScore = event.terrorist_score;
        m_ctScore = event.counter_terrorist_score;
        m_queuePosition = event.queue_position;
        m_phase = event.phase;
        m_notice = event.notice;
        m_seconds = event.seconds;
        if (!event.bomb_planted || !m_planted)
            m_beepTimer = 0.0f;
        m_planted = event.bomb_planted;
        if (m_planted)
            m_bombPosition = {event.bomb_x, event.bomb_y, event.bomb_z};
        m_active = event.active;
        m_alive = event.alive;
        if ((!m_alive || event.phase != "live") && m_using && app.planting_audio)
            app.planting_audio->stop();
        if (m_planted)
        {
            if (!m_bombModel)
            {
                const auto& visual = nx3d::findNXObject("bomb")->visual;
                m_bombModel = std::make_shared<spear::rendering::vulkan::OBJModel>(
                        app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                        app.renderer.getCommandPool(), app.renderer.getGraphicsQueue(),
                        nx3d::projectAssetPath(std::string(visual.worldModel)),
                        nx3d::projectAssetPath(std::string(visual.worldMaterial)),
                        app.descriptorPool, app.descriptorSetLayout,
                        blt::ObjectData(app.shared_world, 0.0f,
                                        glm::vec3(event.bomb_x, event.bomb_y - 64.0f, event.bomb_z),
                                        app.default_size));
                app.scene_manager.getCurrentScene()->addObject(m_bombModel);
            }
            m_bombModel->setPosition({event.bomb_x, event.bomb_y - 64.0f, event.bomb_z});
        }
        else if (m_bombModel)
        {
            app.scene_manager.getCurrentScene()->removeObject(m_bombModel->getId());
            app.pendingDestroy[(app.frameCount - 1) % 3].push_back(std::move(m_bombModel));
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
            m_preservedRound = survivedPreviousRound && !teamChanged ? event.round : 0;
            if (app.movement_controller.isCrouched() && !teamChanged)
                app.camera.setPosition(app.camera.getPosition() +
                                       glm::vec3(0.0f, nx3d::kStandingEyeHeight -
                                                               nx3d::kCrouchingEyeHeight, 0.0f));
            app.movement_controller.resetStance();
            if (teamChanged)
            {
                m_lastSpawnIndex = SIZE_MAX;
                app.camera.setPosition(pickSpawnPoint());
                app.movement_controller.resetVerticalMotion();
                m_prevCamPos = app.camera.getPosition();
            }
            if (!survivedPreviousRound || teamChanged)
            {
                m_weaponPickedUp = true;
                equipWeaponForTeam(app);
            }
        }
    }

    if (m_defusing)
    {
        const glm::vec3 offset = app.camera.getPosition() - m_bombPosition;
        const bool inRange = glm::dot(offset, offset) <= 120.0f * 120.0f;
        if (!m_planted || !m_alive || m_phase != "live" || !inRange)
        {
            app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::matchAction(app.client_api, 0));
            if (app.defusing_audio)
                app.defusing_audio->stop();
            m_defusing = false;
            m_using = false;
            m_defuseSoundTimer = 0.0f;
        }
        else
        {
            m_defuseSoundTimer -= dt;
            if (m_defuseSoundTimer <= 0.0f)
            {
                if (app.defusing_audio)
                    app.defusing_audio->play();
                app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::audioEvent(
                        app.client_api, audio::kSoundDefusing,
                        nexilis::Vector3f({m_bombPosition.x, m_bombPosition.y, m_bombPosition.z})));
                m_defuseSoundTimer = 0.7f;
            }
        }
    }

    if (m_phase == "live" || m_phase == "post")
        m_seconds = std::max(0.0f, m_seconds - dt);
    if (m_phase == "live" && m_planted && m_seconds > 0.0f)
    {
        m_beepTimer -= dt;
        if (m_beepTimer <= 0.0f)
        {
            if (app.bomb_beep_audio)
                app.bomb_beep_audio->playAt(m_bombPosition);
            m_beepTimer = m_seconds <= 5.0f ? 0.28f :
                          m_seconds <= 10.0f ? 0.55f : 0.9f;
        }
    }
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
        status = m_planted ? (m_team == "Counter Terrorist" ?
                                      (app.economyState.objects.count("defuse_kit") > 0 ? "Hold E near bomb to defuse (kit: 2.5s)"
                                                                     : "Hold E near bomb to defuse (5s)")
                                      : "Defend the bomb")
                           : (m_team == "Terrorist" ?
                                      (nx3d::bomb_sites::at(app.camera.getPosition().x,
                                                           standingEyeY(app),
                                                           app.camera.getPosition().z)
                                               ? "Hold E to plant bomb"
                                               : "Plant at bombsite A or B")
                                      : "Stop the plant");
    if (app.ui->matchStatusText.getString() != status)
        app.ui->matchStatusText.setString(status);
    const std::string bombText = m_planted ? "[■] BOMB" : "";
    if (app.ui->bombText.getString() != bombText)
        app.ui->bombText.setString(bombText);
}

void BombMode::handleMouseButtonDown(ClientApp& app, const SDL_Event& event)
{
    if (m_active && m_alive && (m_phase == "live" || m_phase == "post"))
        DeathmatchMode::handleMouseButtonDown(app, event);
}

void BombMode::handleKeyDown(ClientApp& app, const SDL_Event& event)
{
    if (event.key.key == SDLK_G && !event.key.repeat && m_active && m_alive &&
        m_phase == "live" && m_activeSlot == nx3d::findNXObject("bomb")->slot &&
        m_objects.count("bomb") > 0)
    {
        app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::matchAction(
                app.client_api, nx3d::economy::kDropBombAction));
        m_objects.consume("bomb");
        equipSlot(app, !m_primary.type.empty() ? 1 : 2);
        return;
    }
    if (event.key.key == SDLK_E && !event.key.repeat && m_active && m_alive && m_phase == "live")
    {
        const uint8_t action = m_planted ? 2 : 1;
        const auto pos = app.camera.getPosition();
        if ((action == 1 && m_team == "Terrorist" && m_objects.count("bomb") > 0 &&
             nx3d::bomb_sites::at(pos.x, standingEyeY(app), pos.z)) ||
            (action == 2 && m_team == "Counter Terrorist"))
        {
            app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::matchAction(app.client_api, action));
            m_using = true;
            m_defusing = action == 2;
            m_defuseSoundTimer = 0.0f;
            if (action == 1)
            {
                equipSlot(app, 6);
                if (app.planting_audio)
                    app.planting_audio->play();
                app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::audioEvent(
                        app.client_api, audio::kSoundPlanting,
                        nexilis::Vector3f({pos.x, pos.y, pos.z})));
            }
            else if (m_objects.count("defuse_kit") > 0)
                equipSlot(app, 7);
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
        if (m_defusing && app.defusing_audio)
            app.defusing_audio->stop();
        m_defusing = false;
        m_defuseSoundTimer = 0.0f;
        m_using = false;
    }
    DeathmatchMode::handleKeyUp(app, event);
}
} // namespace nx3d::client::gamemode
