#include <server/gamemode/gungame_mode.hh>

#include <shared/gungame.hh>

#include <nexilis/command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>

#include <boost/json/value.hpp>

namespace nx3d::server::gamemode
{
void GunGameMode::onTeamSelected(nexilis::server::Room& room, uint64_t id)
{
    {
        std::lock_guard lock(m_progressMutex);
        m_kills[id] = 0;
        m_lastShotWeapon.erase(id);
    }
    DeathmatchMode::onTeamSelected(room, id);
    m_economy.issueGunGameWeapon(room, id, nx3d::gungame::kWeapons.front());
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
    nexilis::server::Command::ClientMsgType data{
            {"action", "broadcast"}, {"room_id", room.getId()}, {"client_id", 0},
            {"message", boost::json::value(nx3d::gungame::encodeResult(result))}};
    room.broadcastToAll(nexilis::server::Command::clientMessageData(
            nexilis::CommandType::room, "communication", 0, data));
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
