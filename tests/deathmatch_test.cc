#include <server/gamemode/game_mode_registry.hh>

#include <nexilis/client/packet.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/room_storage.hh>

#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

class TestProtocol : public nexilis::Protocol
{
public:
    void start() override {}
    void stop() override {}
    Type getType() override { return Type::BOOST_TCP_SERVER; }
};
} // namespace

int main()
{
    using namespace nexilis;
    using Registry = nx3d::server::gamemode::GameModeRegistry;
    try
    {
        client::ClientConfig config;
        client::ClientAPI shooter(config), victim(config);
        shooter.setClientId(10);
        victim.setClientId(20);
        auto addUser = [](uint64_t id, client::ClientAPI& api)
        {
            auto user = std::make_unique<server::User>(id, "127.0.0.1");
            user->setRoomId(777);
            user->setUsername("player_" + std::to_string(id));
            user->setBoostTCPSend([&api](const nx_data& data)
            {
                require(api.readMessage(data) == client::ReadResult::success,
                        "Client rejected deathmatch message");
            });
            server::ClientStorage::add(std::move(user));
        };
        addUser(10, shooter);
        addUser(20, victim);

        // Register callbacks before adding more rooms, as the real server does.
        for (uint64_t id = 777; id < 780; ++id)
        {
            server::RoomStorage::add(server::Room(RoomData(10, "test", id)));
            Registry::assignRoom(*server::RoomStorage::getRoomById(id), nx3d::GameMode::deathmatch);
        }
        auto* room = server::RoomStorage::getRoomById(777);
        room->joinRoom(10);
        room->joinRoom(20);
        room->setPlayerTeam(10, "Terrorist");
        room->setPlayerTeam(20, "Counter Terrorist");
        server::ServerConfig settings;
        server::Command command(settings);
        TestProtocol protocol;

        for (uint64_t kill = 1; kill <= 100; ++kill)
        {
            for (int shot = 0; shot < 3; ++shot)
            {
                auto packet = client::Packet::Room::Player3D::shoot(shooter, 20, 35.0f);
                nx_data payload(packet.begin() + 16, packet.end());
                require(command.read(payload, *server::ClientStorage::getClientById(10), protocol, 1)
                            == server::CommandResult::success, "Shot rejected");
            }
            require(room->getPlayerHealth(20) == 100.0f, "Victim was not revived");
            require(room->getPlayerKills(10) == kill, "Kill count incorrect");
            require(room->getPlayerDeaths(20) == kill, "Death count incorrect");
            for (auto* api : {&shooter, &victim})
            {
                auto damage = api->consumeDamageEvents();
                require(damage.size() == 3 && damage.back().new_health == 0.0f, "Damage events incorrect");
                auto respawns = api->consumeRespawnEvents();
                require(respawns.size() == 1 && respawns.front().target_id == 20, "Respawn event incorrect");
                auto boards = api->consumeLeaderboardEvents();
                require(boards.size() == 1 && boards.front().entries.size() == 2, "Leaderboard delta incorrect");
                require(boards.front().entries[0].kills == kill, "Client kill count incorrect");
                require(boards.front().entries[1].deaths == kill, "Client death count incorrect");
                require(api->consumeRespawnEvents().empty(), "Respawn consumed twice");
            }
        }
        Registry::clear();
        server::RoomStorage::clear();
        server::ClientStorage::clear();
        std::cout << "100 death/respawn cycles passed for both clients\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
