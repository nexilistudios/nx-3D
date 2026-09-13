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

#include <server/gamemode/game_mode_registry.hh>
#include <server/gun/gun_manager.hh>

#include <shared/gamemode.hh>

#include <functional>
#include <iostream>

namespace
{

/// Create a room with the given display name.
nexilis::server::Room createRoom(const std::string& name)
{
    auto roomData = nexilis::RoomData(0, name, nexilis::Util::getRandomUint64());
    return nexilis::server::Room(roomData);
}

} // namespace

int main()
{
    nexilis::Log::startConsoleDebugging();

    using namespace nexilis::server;

    ServerConfig server_config;
    server_config.setMode(AuthenticationMode::password_protected);
    server_config.setPassphrase("password");
    server_config.setRootPassword("root");

    namespace gamemode = nx3d::server::gamemode;

    // Some initial rooms. The gamemode is decided here on the server, purely as
    // game logic: nexilis does not know about gamemodes, so the registry keeps
    // track of which room runs which mode.
    {
        auto room = createRoom("Room 1");
        auto roomId = room.getId();
        RoomStorage::add(std::move(room));
        gamemode::GameModeRegistry::assignRoom(*RoomStorage::getRoomById(roomId), nx3d::GameMode::deathmatch);
    }

    {
        auto room = createRoom("Room 2");
        auto roomId = room.getId();
        RoomStorage::add(std::move(room));
        gamemode::GameModeRegistry::assignRoom(*RoomStorage::getRoomById(roomId), nx3d::GameMode::deathmatch);
    }

    {
        auto room = createRoom("Room 3");
        auto roomId = room.getId();
        RoomStorage::add(std::move(room));
        gamemode::GameModeRegistry::assignRoom(*RoomStorage::getRoomById(roomId), nx3d::GameMode::deathmatch);
    }

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
        gamemode::GameModeRegistry::update(1.0f / 60.0f);
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