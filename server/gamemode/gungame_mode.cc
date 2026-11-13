#include <server/gamemode/gungame_mode.hh>

#include <shared/gungame.hh>

#include <nexilis/command_type.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>

#include <boost/json/value.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace nx3d::server::gamemode
{
namespace
{
void broadcastCountdown(nexilis::server::Room& room, int seconds)
{
    nexilis::server::Command::ClientMsgType data{
            {"action", "broadcast"}, {"room_id", room.getId()}, {"client_id", 0},
            {"message", boost::json::value(nx3d::gungame::encodeCountdown(seconds))}};
    room.broadcastToAll(nexilis::server::Command::clientMessageData(
            nexilis::CommandType::room, "communication", 0, data));
}
} // namespace

void GunGameMode::onTeamSelected(nexilis::server::Room& room, uint64_t id)
{
    int countdown = 0;
    {
        std::lock_guard lock(m_progressMutex);
        m_kills[id] = 0;
        m_lastShotWeapon.erase(id);
        if (m_finished)
            countdown = m_announcedSeconds;
    }
    DeathmatchMode::onTeamSelected(room, id);
    m_economy.issueGunGameWeapon(room, id, nx3d::gungame::kWeapons.front());
    if (countdown > 0)
        broadcastCountdown(room, countdown);
}

void GunGameMode::onPlayerDeath(nexilis::server::Room& room, uint64_t killer,
                                uint64_t victim)
{
    DeathmatchMode::onPlayerDeath(room, killer, victim);
    if (killer == 0 || killer == victim || room.getPlayerTeam(killer).empty())
        return;
    int kills = 0;
    bool won = false;
    std::unordered_map<uint64_t, int> standings;
    {
        std::lock_guard lock(m_progressMutex);
        if (m_finished)
            return;
        // A launcher blast can kill more than one player. Once the shooter
        // reaches the knife stage, a remaining blast must not win the game.
        if (nx3d::gungame::weaponForKills(m_kills[killer]) == "knife" &&
            m_lastShotWeapon[killer] != "knife")
            return;
        kills = ++m_kills[killer];
        won = kills >= nx3d::gungame::kKillsToWin;
        if (won)
        {
            m_finished = true;
            m_restartSeconds = static_cast<float>(nx3d::gungame::kRestartSeconds);
            m_announcedSeconds = nx3d::gungame::kRestartSeconds;
            standings = m_kills;
        }
    }
    if (!won)
    {
        m_economy.issueGunGameWeapon(room, killer,
                                     nx3d::gungame::weaponForKills(kills));
        return;
    }

    nx3d::gungame::Result result;
    result.winner = killer;
    for (auto id : room.getClients())
    {
        const auto user = nexilis::server::ClientStorage::getClientById(id);
        if (user)
            result.standings.push_back({id, standings[id], user->getUsername()});
    }
    nx3d::gungame::rankStandings(result.standings);
    broadcastCountdown(room, nx3d::gungame::kRestartSeconds);
    nexilis::server::Command::ClientMsgType data{
            {"action", "broadcast"}, {"room_id", room.getId()}, {"client_id", 0},
            {"message", boost::json::value(nx3d::gungame::encodeResult(result))}};
    room.broadcastToAll(nexilis::server::Command::clientMessageData(
            nexilis::CommandType::room, "communication", 0, data));
}

void GunGameMode::update(nexilis::server::Room& room, float dt)
{
    DeathmatchMode::update(room, dt);

    int countdown = -1;
    bool restart = false;
    {
        std::lock_guard lock(m_progressMutex);
        if (!m_finished || m_restarting)
            return;
        m_restartSeconds = std::max(0.0f, m_restartSeconds - dt);
        const int remaining = static_cast<int>(std::ceil(m_restartSeconds));
        if (remaining != m_announcedSeconds)
        {
            m_announcedSeconds = remaining;
            countdown = remaining;
        }
        if (remaining == 0)
        {
            m_restarting = true;
            restart = true;
        }
    }
    if (countdown >= 0)
        broadcastCountdown(room, countdown);
    if (!restart)
        return;

    const auto players = room.getClients();
    {
        std::lock_guard lock(m_progressMutex);
        m_kills.clear();
        m_lastShotWeapon.clear();
    }
    room.resetPlayerStats();
    m_economy.resetMatch(room, std::unordered_set<uint64_t>(players.begin(), players.end()));
    for (auto id : players)
    {
        if (room.getPlayerTeam(id).empty())
            continue;
        room.resetPlayerHealth(id);
        onTeamSelected(room, id);
        auto player = nexilis::server::ClientStorage::getClientById(id);
        if (!player)
            continue;
        nexilis::nx_data respawnData{
                static_cast<uint8_t>(nexilis::CommandType::room),
                static_cast<uint8_t>(nexilis::RoomCommandType::Root::player_3D),
                static_cast<uint8_t>(nexilis::RoomCommandType::PlayerType::respawn)};
        room.broadcastToAll(nexilis::server::Command::createRoomCommand(
                room.getId(), *player, respawnData, {{"target_id", id}}, 0));
    }
    if (!players.empty())
    {
        auto sender = nexilis::server::ClientStorage::getClientById(players.front());
        if (sender)
            room.broadcastToAll(nexilis::server::Command::createRoomLeaderboardCommand(
                    room.getId(), *sender,
                    nexilis::server::Command::playerStatsEntries(room, players), 0));
    }
    {
        std::lock_guard lock(m_progressMutex);
        m_finished = false;
        m_restarting = false;
    }
}

void GunGameMode::afterRespawn(nexilis::server::Room& room, uint64_t id)
{
    int kills;
    {
        std::lock_guard lock(m_progressMutex);
        kills = m_kills[id];
    }
    m_economy.issueGunGameWeapon(room, id, nx3d::gungame::weaponForKills(kills));
}

void GunGameMode::onAcceptedShot(uint64_t id, const std::string& weapon)
{
    std::lock_guard lock(m_progressMutex);
    m_lastShotWeapon[id] = weapon;
}

std::string GunGameMode::weaponForSlot(uint64_t id, uint8_t slot) const
{
    std::lock_guard lock(m_progressMutex);
    const auto it = m_kills.find(id);
    if (it == m_kills.end())
        return {};
    const auto stage = nx3d::gungame::weaponForKills(it->second);
    return slot == (stage == "knife" ? 3 : 1) ? std::string(stage) : std::string{};
}
} // namespace nx3d::server::gamemode
