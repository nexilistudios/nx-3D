#include <nexilis/command_type.hh>
#include <nexilis/object/game_item.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/room_data.hh>

#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>
#include <nexilis/server/protocol/nxboost/udp_server.hh>

#include <nexilis/server/room_storage.hh>
#include <nexilis/server/runtime.hh>
#include <nexilis/server/server_config.hh>

#include <server/gun/gun_manager.hh>

#include <iostream>

static void spawnGameItems()
{
    using namespace nexilis;
    using namespace nexilis::server;

    auto& rooms = RoomStorage::getAllRooms();
    for (auto& room : rooms)
    {
        room.addGameItem(GameItem(
                Util::getRandomUint64(),
                "ak47",
                Vector3f{-1600.0f, 60.0f, -2400.0f},
                Vector3f{33.0f, 33.0f, 33.0f},
                "on_ground",
                ""));
        std::cout << "[GameItem] Spawned AK-47 in room: " << room.getName() << std::endl;
    }
}

static void setupDeathHandlers()
{
    using namespace nexilis;
    using namespace nexilis::server;

    auto& rooms = RoomStorage::getAllRooms();
    for (auto& room : rooms)
    {
        room.setDeathHandler([](Room& room, uint64_t killerId, uint64_t victimId)
        {
            room.resetPlayerHealth(victimId);

            auto* killer = ClientStorage::getClientById(killerId);
            if (!killer)
                return;

            nx_data respawnData;
            respawnData.emplace_back(static_cast<uint8_t>(CommandType::room));
            respawnData.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
            respawnData.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::respawn));

            std::map<std::string, boost::json::value> respawnParams{
                    {"target_id", boost::json::value(victimId)}};

            auto packet = Command::createRoomCommand(
                    room.getId(), *killer, respawnData, respawnParams, 0);
            room.broadcastToAll(packet);

            std::cout << "[Death] Player " << victimId
                      << " killed by " << killerId
                      << " in room: " << room.getName()
                      << " - respawned" << std::endl;
        });
    }
}

int main()
{
    nexilis::Log::startConsoleDebugging();

    using namespace nexilis::server;

    ServerConfig server_config;
    server_config.setMode(AuthenticationMode::password_protected);
    server_config.setPassphrase("password");
    server_config.setRootPassword("root");

    // Some initial rooms.
    auto room1 =
            Room(nexilis::RoomData(0, "Room 1", nexilis::Util::getRandomUint64(),
                                   nexilis::RoomData::Context::_3D));
    RoomStorage::add(std::move(room1));

    auto room2 =
            Room(nexilis::RoomData(0, "Room 2", nexilis::Util::getRandomUint64(),
                                   nexilis::RoomData::Context::_3D));
    RoomStorage::add(std::move(room2));

    auto room3 =
            Room(nexilis::RoomData(0, "Room 3", nexilis::Util::getRandomUint64(),
                                   nexilis::RoomData::Context::_3D));
    RoomStorage::add(std::move(room3));

    spawnGameItems();
    setupDeathHandlers();

    nexilis::ProtocolManager protocolManager;

    // Boost TCP server
    auto boostTCPServer =
            protocolManager.createProtocol<nxboost::TCPServer>(server_config);
    boostTCPServer.start();

    auto boostUDPServer =
            protocolManager.createProtocol<nxboost::UDPServer>(server_config);
    boostUDPServer.start();

    // clang-format off
    auto condition = [](size_t){ return true; };

    // Gun system
    nx3d::server::gun::GunManager gunManager;
    std::cout << "[Guns] Loaded " << gunManager.getDefs().size() << " gun definitions" << std::endl;
    for (auto& def : gunManager.getDefs())
        std::cout << "  - " << def.name << " (dmg:" << def.damage
                  << " rpm:" << def.fire_rate * 60.0f
                  << " mag:" << def.magazine_size << ")" << std::endl;

    auto f = std::function<bool()>([&gunManager]()
    {
        gunManager.update(1.0f / 60.0f);
        return true;
    });

    auto server_runtime = std::thread([&condition, &f](){ nexilis::server::runtime(condition, f, 1); });
    server_runtime.detach();
    // clang-format on

    // Wait for user input to stop the server
    std::cout << "Nx3D game server is running. Press Enter to stop..."
              << std::endl;
    std::cin.get();
}
