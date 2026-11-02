#include <nexilis/command_type.hh>
#include <nexilis/object/game_item.hh>
#include <nexilis/ports.hh>
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

#include <nexilis/server/signal.hh>
#include <server/gamemode/game_mode_registry.hh>
#include <server/gun/gun_manager.hh>

#include <shared/gamemode.hh>
#include <shared/brush_map.hh>
#include <shared/network_ports.hh>
#include <shared/player_rules.hh>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <pthread.h>

namespace
{

/// Create a room with the given display name.
nexilis::server::Room createRoom(const std::string& name)
{
    auto roomData = nexilis::RoomData(0, name, nexilis::Util::getRandomUint64());
    return nexilis::server::Room(roomData);
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        const auto ports = nx3d::parseNetworkPorts(argc, argv);
        nexilis::Ports::setBoostTCPPort(ports.tcp);
        nexilis::Ports::setBoostUDPPort(ports.udp);
        nexilis::Ports::setBoostTCPSessionPort(ports.tcpSession);
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "Invalid network port: " << error.what() << std::endl;
        return 1;
    }

    nexilis::Log::startConsoleLogging(nexilis::logger::LogLevel::Info);

    using namespace nexilis::server;
    SignalHandler signal_handler;

    ServerConfig server_config;
    server_config.setMode(AuthenticationMode::password_protected);
    server_config.setPassphrase("password");
    server_config.setRootPassword("root");
    auto& limits = server_config.getGameplayLimits();
    limits.setMaxHorizontalPosition(nx3d::kMaxHorizontalPosition);
    limits.setMinHeight(nx3d::kMinPlayerHeight);
    limits.setMaxHeight(nx3d::kMaxPlayerHeight);
    limits.setMaxMovementSpeed(nx3d::kMaxMovementSpeed);
    limits.setMaxShootingRate(nx3d::kMaxShootingRate);
    limits.setMaxDamagePerShot(nx3d::kMaxDamagePerShot);

    namespace gamemode = nx3d::server::gamemode;

    // The first room uses Shipment; the second uses Dust II. Room names are
    // advertised to clients and identify the bundled map to load.
    {
        auto room = createRoom(std::string(nx3d::kShipmentRoomName));
        auto roomId = room.getId();
        RoomStorage::add(std::move(room));
        gamemode::GameModeRegistry::assignRoom(*RoomStorage::getRoomById(roomId),
                                                nx3d::GameMode::deathmatch);
    }

    {
        auto room = createRoom(std::string(nx3d::kDust2RoomName));
        auto roomId = room.getId();
        RoomStorage::add(std::move(room));
        gamemode::GameModeRegistry::assignRoom(*RoomStorage::getRoomById(roomId),
                                                nx3d::GameMode::bomb);
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

    auto lastTick = std::chrono::steady_clock::now();
    std::atomic<bool> running{true};
    auto f = std::function<bool()>([&gunManager, &lastTick, &running]()
    {
        if (!running.load())
            return false;
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;
        gunManager.update(dt);
        gamemode::GameModeRegistry::update(dt);
        return true;
    });

    auto server_runtime = std::thread([&condition, &f](){ nexilis::server::runtime(condition, f, 60); });
    // clang-format on

    std::cout << "Nx3D game server is running. Waiting for SIGINT or SIGTERM..."
              << std::endl;

    signal_handler.waitForShutdown();
    running.store(false);
    server_runtime.join();
}
