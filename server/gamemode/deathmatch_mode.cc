#include <server/gamemode/deathmatch_mode.hh>

#include <nexilis/command_type.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>

#include <boost/json/value.hpp>

#include <iostream>
#include <map>

namespace nx3d::server::gamemode
{

nx3d::GameMode DeathmatchMode::type() const
{
    return nx3d::GameMode::deathmatch;
}

void DeathmatchMode::onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId)
{
    using namespace nexilis;

    room.resetPlayerHealth(victimId);

    auto* killer = nexilis::server::ClientStorage::getClientById(killerId);
    if (!killer)
        return;

    nx_data respawnData;
    respawnData.emplace_back(static_cast<uint8_t>(CommandType::room));
    respawnData.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    respawnData.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::respawn));

    std::map<std::string, boost::json::value> respawnParams{
            {"target_id", boost::json::value(victimId)}};

    auto packet = nexilis::server::Command::createRoomCommand(
            room.getId(), *killer, respawnData, respawnParams, 0);
    room.broadcastToAll(packet);

    std::cout << "[Deathmatch] Player " << victimId
              << " killed by " << killerId
              << " in room: " << room.getName()
              << " - respawned" << std::endl;
}

} // namespace nx3d::server::gamemode