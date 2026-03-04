#include <nexilis/protocol_manager.hh>
#include <nexilis/room_data.hh>

#include <nexilis/server/protocol/nxboost/tcp_server.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/runtime.hh>

#include <iostream>

int main()
{
    nexilis::Log::startConsoleDebugging();

    using namespace nexilis::server;

    Settings auth;
    auth.setMode(AuthenticationMode::password_protected);
    auth.setPassphrase("password");
    auth.setRootPassword("root");

    // Create a room.
    auto room =
            Room(nexilis::RoomData(0, "ExampleRoom", nexilis::Util::getRandomUint64(),
                                   nexilis::RoomData::Context::_3D));
    auto id = room.getId();
    RoomStorage::add(std::move(room));
    assert(RoomStorage::contains(id));

    nexilis::ProtocolManager protocolManager;

    // Boost TCP server
    auto boostTCPServer =
            protocolManager.createProtocol<nxboost::TCPServer>(auth);
    boostTCPServer.start();

    // clang-format off
    auto condition = [](size_t){ return true; };

    auto f = std::function<bool()>([]()
    {
        std::cout << "Updating nx3D server" << std::endl;
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
