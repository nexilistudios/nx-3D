#include <server/gamemode/deathmatch_mode.hh>

#include <nexilis/command_type.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>

#include <boost/json/value.hpp>

#include <algorithm>
#include <iostream>
#include <map>

namespace nx3d::server::gamemode
{

nx3d::GameMode DeathmatchMode::type() const
{
    return nx3d::GameMode::deathmatch;
}

void DeathmatchMode::onTeamSelected(nexilis::server::Room& room, uint64_t playerId)
{
    m_economy.joined(room, playerId);
}

bool DeathmatchMode::onAction(nexilis::server::Room& room, uint64_t playerId, uint8_t action)
{
    if (action == nx3d::economy::kThrowGrenadeAction || action == nx3d::economy::kThrowSmokeAction)
        return m_economy.throwGrenade(room, playerId, room.getPlayerHealth(playerId) > 0.0f,
                                      action == nx3d::economy::kThrowSmokeAction);
    if (action == nx3d::economy::kDropPrimaryAction ||
        action == nx3d::economy::kDropSecondaryAction)
        return m_economy.dropWeapon(room, playerId, action);
    return m_economy.buy(room, playerId, action,
                         room.getPlayerHealth(playerId) > 0.0f, false);
}

void DeathmatchMode::onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId)
{
    using namespace nexilis;

    room.recordKill(killerId, victimId);
    m_economy.died(room, killerId, victimId);
    {
        std::lock_guard lock(m_respawnMutex);
        m_respawnTimers[victimId] = 2.0f;
    }

    auto* sender = nexilis::server::ClientStorage::getClientById(
            killerId == 0 ? victimId : killerId);
    if (!sender)
        return;

    // Notify the room about the kill: just the two affected players' updated
    // stats. Clients merge these into their locally-maintained leaderboard.
    auto statsPayload = nexilis::server::Command::createRoomLeaderboardCommand(
            room.getId(), *sender,
            nexilis::server::Command::playerStatsEntries(
                    room, killerId == 0 ? std::vector<uint64_t>{victimId}
                                        : std::vector<uint64_t>{killerId, victimId}),
            0);
    room.broadcastToAll(statsPayload);

    std::cout << "[Deathmatch] Player " << victimId
              << " killed by " << killerId
              << " in room: " << room.getName()
              << " - respawning soon" << std::endl;
}

void DeathmatchMode::update(nexilis::server::Room& room, float dt)
{
    m_economy.updateGrenades(room, dt);
    std::vector<uint64_t> ready;
    const auto clients = room.getClients();
    m_economy.reconcile(clients);
    {
        std::lock_guard lock(m_respawnMutex);
        for (auto it = m_respawnTimers.begin(); it != m_respawnTimers.end();)
        {
            if (std::find(clients.begin(), clients.end(), it->first) == clients.end())
            {
                it = m_respawnTimers.erase(it);
                continue;
            }
            it->second -= dt;
            if (it->second <= 0.0f)
            {
                ready.push_back(it->first);
                it = m_respawnTimers.erase(it);
            }
            else
                ++it;
        }
    }
    for (auto victimId : ready)
    {
        auto* sender = nexilis::server::ClientStorage::getClientById(victimId);
        if (!sender)
            continue;
        room.resetPlayerHealth(victimId);
        m_economy.respawned(room, victimId);
        nexilis::nx_data respawnData{
                static_cast<uint8_t>(nexilis::CommandType::room),
                static_cast<uint8_t>(nexilis::RoomCommandType::Root::player_3D),
                static_cast<uint8_t>(nexilis::RoomCommandType::PlayerType::respawn)};
        std::map<std::string, boost::json::value> params{
                {"target_id", boost::json::value(victimId)}};
        room.broadcastToAll(nexilis::server::Command::createRoomCommand(
                room.getId(), *sender, respawnData, params, 0));
    }
}

} // namespace nx3d::server::gamemode
