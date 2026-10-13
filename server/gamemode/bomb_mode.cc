#include <server/gamemode/bomb_mode.hh>

#include <nexilis/command_type.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace nx3d::server::gamemode
{
namespace
{
constexpr const char* kT = "Terrorist";
constexpr const char* kCT = "Counter Terrorist";
constexpr float kRoundSeconds = 115.0f;
constexpr float kBombSeconds = 40.0f;
constexpr float kPlantSeconds = 3.0f;
constexpr float kDefuseSeconds = 5.0f;
constexpr float kBetweenSeconds = 4.0f;
constexpr float kDefuseRange = 120.0f;
} // namespace

void BombMode::publish(nexilis::server::Room& room)
{
    using nexilis::server::ClientStorage;
    using nexilis::server::Command;
    for (auto id : room.getClients())
    {
        auto* user = ClientStorage::getClientById(id);
        if (!user || !user->isBoostTCPSet())
            continue;
        auto queueIt = std::find(m_queue.begin(), m_queue.end(), id);
        const auto position = queueIt == m_queue.end() ? 0u
                                                       : static_cast<unsigned>(std::distance(m_queue.begin(), queueIt) + 1);
        const char* phase = m_phase == Phase::live ? "live" : m_phase == Phase::post ? "post"
                                                      : m_phase == Phase::finished   ? "finished"
                                                                                     : "waiting";
        Command::ClientMsgType data{
                {"action", "state"}, {"room_id", room.getId()}, {"round", m_round}, {"terrorist_score", m_tScore}, {"counter_terrorist_score", m_ctScore}, {"queue_position", position}, {"phase", phase}, {"team", boost::json::value(room.getPlayerTeam(id))}, {"notice", boost::json::value(m_notice)}, {"seconds", static_cast<double>(std::max(0.0f, m_seconds))}, {"bomb_planted", m_planted}, {"bomb_x", static_cast<double>(m_bombX)}, {"bomb_y", static_cast<double>(m_bombY)}, {"bomb_z", static_cast<double>(m_bombZ)}, {"active", m_players.contains(id)}, {"alive", m_alive.contains(id)}};
        user->boostTCPSend(Command::clientMessageData(nexilis::CommandType::room, "match", 0, data));
    }
}

void BombMode::clearAction()
{
    m_actionPlayer = 0;
    m_action = 0;
    m_actionSeconds = 0.0f;
}

void BombMode::onTeamSelected(nexilis::server::Room& room, uint64_t playerId)
{
    m_economy.joined(room, playerId);
    // Halftime calls Room::setPlayerTeam while update() already holds m_mutex.
    if (m_swapping.load())
        return;
    std::lock_guard lock(m_mutex);
    if (m_swapping)
        return;
    if (!m_players.contains(playerId) &&
        std::find(m_queue.begin(), m_queue.end(), playerId) == m_queue.end())
    {
        m_queue.push_back(playerId);
        // A queued player is a spectator until admitted at a round boundary.
        room.damagePlayer(playerId, 100.0f);
    }
    if (m_phase == Phase::waiting)
    {
        bool hasT = false;
        bool hasCT = false;
        for (auto id : room.getClients())
        {
            hasT |= room.getPlayerTeam(id) == kT;
            hasCT |= room.getPlayerTeam(id) == kCT;
        }
        if (!hasT || !hasCT)
            m_notice = std::string("Cannot start game because ") +
                       (!hasT ? kT : kCT) + " team is empty";
        else
            m_notice = "Starting round";
    }
    publish(room);
}

bool BombMode::canDamage(uint64_t shooterId, uint64_t targetId) const
{
    std::lock_guard lock(m_mutex);
    return m_phase == Phase::live && m_alive.contains(shooterId) && m_alive.contains(targetId);
}

bool BombMode::canSelectTeam(uint64_t playerId) const
{
    (void)playerId;
    return true;
}

bool BombMode::teamAlive(nexilis::server::Room& room, const std::string& team) const
{
    for (auto id : m_alive)
        if (room.getPlayerTeam(id) == team)
            return true;
    return false;
}

bool BombMode::closeToBomb(uint64_t playerId) const
{
    auto* user = nexilis::server::ClientStorage::getClientById(playerId);
    if (!user)
        return false;
    auto pos = user->getObject3D().getPosition();
    const float dx = pos.x - m_bombX, dy = pos.y - m_bombY, dz = pos.z - m_bombZ;
    return dx * dx + dy * dy + dz * dz <= kDefuseRange * kDefuseRange;
}

bool BombMode::onAction(nexilis::server::Room& room, uint64_t playerId, uint8_t action)
{
    std::lock_guard lock(m_mutex);
    if (nx3d::economy::itemForAction(action))
        return m_economy.buy(room, playerId, action,
                             m_phase == Phase::live && m_alive.contains(playerId));
    if (action == nx3d::economy::kThrowGrenadeAction || action == nx3d::economy::kThrowSmokeAction)
        return m_economy.throwGrenade(room, playerId,
                                     m_phase == Phase::live && m_alive.contains(playerId),
                                     action == nx3d::economy::kThrowSmokeAction);
    if (action == nx3d::economy::kDropPrimaryAction ||
        action == nx3d::economy::kDropSecondaryAction)
        return m_economy.dropWeapon(room, playerId, action);
    if (action == 0)
    {
        if (m_actionPlayer == playerId)
            clearAction();
        return true;
    }
    if (m_phase != Phase::live || !m_alive.contains(playerId))
        return false;
    const auto team = room.getPlayerTeam(playerId);
    if ((action == 1 && (!m_planted && team == kT)) ||
        (action == 2 && m_planted && team == kCT && closeToBomb(playerId)))
    {
        if (m_actionPlayer != playerId || m_action != action)
        {
            m_actionPlayer = playerId;
            m_action = action;
            m_actionSeconds = 0.0f;
        }
        return true;
    }
    return false;
}

void BombMode::onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId)
{
    std::lock_guard lock(m_mutex);
    if (m_phase != Phase::live || !m_alive.erase(victimId))
        return;
    if (m_actionPlayer == victimId)
        clearAction();
    m_economy.died(room, killerId, victimId);
    room.recordKill(killerId, victimId);
    auto* sender = nexilis::server::ClientStorage::getClientById(
            killerId == 0 ? victimId : killerId);
    if (sender)
        room.broadcastToAll(nexilis::server::Command::createRoomLeaderboardCommand(
                room.getId(), *sender,
                nexilis::server::Command::playerStatsEntries(
                        room, killerId == 0 ? std::vector<uint64_t>{victimId}
                                            : std::vector<uint64_t>{killerId, victimId}),
                0));
    if (!teamAlive(room, kCT))
        endRound(room, true, "Counter Terrorists eliminated");
    else if (!teamAlive(room, kT) && !m_planted)
        endRound(room, false, "Terrorists eliminated");
    publish(room);
}

void BombMode::endRound(nexilis::server::Room& room, bool terroristsWin, const std::string& reason)
{
    if (m_phase != Phase::live)
        return;
    if (terroristsWin)
        ++m_tScore;
    else
        ++m_ctScore;
    m_phase = Phase::post;
    m_seconds = kBetweenSeconds;
    m_notice = std::string(terroristsWin ? "T win: " : "CT win: ") + reason;
    for (auto id : m_players)
    {
        const auto team = room.getPlayerTeam(id);
        if (team != kT && team != kCT)
            continue;
        const bool won = (team == kT) == terroristsWin;
        m_economy.reward(room, id, won ? nx3d::economy::kRoundWinReward
                                      : nx3d::economy::kRoundLossReward,
                         won ? "round win" : "round loss");
    }
    clearAction();
    publish(room);
}

void BombMode::startRound(nexilis::server::Room& room)
{
    if (m_round == 12 && m_phase == Phase::post)
    {
        m_swapping = true;
        for (auto id : m_players)
        {
            const auto team = room.getPlayerTeam(id);
            room.setPlayerTeam(id, team == kT ? kCT : kT);
        }
        m_swapping = false;
        std::swap(m_tScore, m_ctScore);
        auto* user = m_players.empty() ? nullptr
                                       : nexilis::server::ClientStorage::getClientById(*m_players.begin());
        if (user)
            room.broadcastToAll(nexilis::server::Command::createRoomLeaderboardCommand(
                    room.getId(), *user,
                    nexilis::server::Command::playerStatsEntries(room, room.getClients()), 0));
    }

    // Admit every queued player at the round boundary. Players arriving during
    // a live round still wait until the current round is over.
    for (auto it = m_queue.begin(); it != m_queue.end();)
    {
        const auto team = room.getPlayerTeam(*it);
        if (team == kT || team == kCT)
        {
            m_players.insert(*it);
            it = m_queue.erase(it);
        }
        else
            ++it;
    }

    bool hasT = false, hasCT = false;
    for (auto id : m_players)
    {
        hasT |= room.getPlayerTeam(id) == kT;
        hasCT |= room.getPlayerTeam(id) == kCT;
    }
    if (!hasT || !hasCT)
    {
        m_phase = Phase::waiting;
        m_notice = std::string("Cannot start game because ") +
                   (!hasT ? kT : kCT) + " team is empty";
        m_seconds = 0.0f;
        publish(room);
        return;
    }

    ++m_round;
    m_phase = Phase::live;
    m_seconds = kRoundSeconds;
    m_planted = false;
    m_notice = "Round live";
    clearAction();
    m_alive = m_players;
    for (auto id : m_players)
    {
        room.resetPlayerHealth(id);
        m_economy.respawned(room, id);
        auto* user = nexilis::server::ClientStorage::getClientById(id);
        if (!user)
            continue;
        nexilis::nx_data bytes{
                static_cast<uint8_t>(nexilis::CommandType::room),
                static_cast<uint8_t>(nexilis::RoomCommandType::Root::player_3D),
                static_cast<uint8_t>(nexilis::RoomCommandType::PlayerType::respawn)};
        room.broadcastToAll(nexilis::server::Command::createRoomCommand(
                room.getId(), *user, bytes, {{"target_id", id}}, 0));
    }
    publish(room);
}

void BombMode::reconcile(nexilis::server::Room& room)
{
    const auto& clients = room.getClients();
    m_economy.reconcile(clients);
    auto present = [&clients](uint64_t id)
    {
        return std::find(clients.begin(), clients.end(), id) != clients.end();
    };
    std::erase_if(m_queue, [&](uint64_t id)
                  { return !present(id); });
    std::erase_if(m_players, [&](uint64_t id)
                  { return !present(id); });
    std::erase_if(m_alive, [&](uint64_t id)
                  { return !present(id); });
    if (!present(m_actionPlayer))
        clearAction();
}

void BombMode::update(nexilis::server::Room& room, float dt)
{
    m_economy.updateGrenades(room, dt);
    std::lock_guard lock(m_mutex);
    reconcile(room);
    m_publishClock += dt;
    if (m_phase == Phase::waiting && m_publishClock >= 1.0f)
        startRound(room);
    else if (m_phase == Phase::live)
    {
        m_seconds -= dt;
        if (m_actionPlayer && m_alive.contains(m_actionPlayer))
        {
            if (m_action == 2 && !closeToBomb(m_actionPlayer))
                clearAction();
            else
            {
                m_actionSeconds += dt;
                if (m_action == 1 && m_actionSeconds >= kPlantSeconds)
                {
                    auto* user = nexilis::server::ClientStorage::getClientById(m_actionPlayer);
                    if (user)
                    {
                        auto pos = user->getObject3D().getPosition();
                        m_bombX = pos.x;
                        m_bombY = pos.y;
                        m_bombZ = pos.z;
                        m_planted = true;
                        m_seconds = kBombSeconds;
                        m_notice = "Bomb planted";
                        m_economy.reward(room, m_actionPlayer, nx3d::economy::kPlantReward,
                                         "bomb planted");
                    }
                    clearAction();
                    publish(room);
                }
                else if (m_action == 2 && m_actionSeconds >=
                         (m_economy.hasDefuseKit(m_actionPlayer) ? kDefuseSeconds * 0.5f : kDefuseSeconds))
                {
                    m_economy.reward(room, m_actionPlayer, nx3d::economy::kDefuseReward,
                                     "bomb defused");
                    m_planted = false;
                    endRound(room, false, "Bomb defused");
                }
            }
        }
        if (m_phase == Phase::live)
        {
            if (!teamAlive(room, kCT))
                endRound(room, true, "Counter Terrorists eliminated");
            else if (!teamAlive(room, kT) && !m_planted)
                endRound(room, false, "Terrorists eliminated");
            else if (m_seconds <= 0.0f)
                endRound(room, m_planted, m_planted ? "Bomb exploded" : "Time expired");
        }
    }
    else if (m_phase == Phase::post)
    {
        m_seconds -= dt;
        if (m_seconds <= 0.0f)
        {
            if (m_tScore >= 13 || m_ctScore >= 13 || m_round >= 24)
            {
                m_phase = Phase::finished;
                m_seconds = 10.0f;
                m_notice = m_tScore == m_ctScore ? "Match drawn" : "Match finished";
            }
            else
                startRound(room);
            publish(room);
        }
    }
    else if (m_phase == Phase::finished)
    {
        m_seconds -= dt;
        if (m_seconds <= 0.0f)
        {
            m_round = m_tScore = m_ctScore = 0;
            m_phase = Phase::waiting;
            m_economy.resetMatch(room, m_players);
            startRound(room);
        }
    }
    if (m_publishClock >= 1.0f)
    {
        m_publishClock = 0.0f;
        publish(room);
    }
}
} // namespace nx3d::server::gamemode
