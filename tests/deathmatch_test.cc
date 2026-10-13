#include <server/gamemode/game_mode_registry.hh>
#include <shared/fall_death.hh>
#include <shared/economy.hh>

#include <nexilis/client/packet.hh>
#include <nexilis/command_type.hh>
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

class TestProtocol : public nexilis::NXProtocol
{
public:
    void start() override
    {
    }
    void stop() override
    {
    }
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::BOOST_TCP_SERVER;
    }
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
            require(api.readMessage(server::Command::clientMessageData(
                            CommandType::room, "management", 0,
                            {{"action", "create"}, {"room_id", 777}, {"client_id", id},
                             {"room_name", "test"}, {"room_context", 1}})) == client::ReadResult::success,
                    "Test client could not join room messages");
            auto user = std::make_unique<server::User>(id, "127.0.0.1");
            user->setRoomId(777);
            user->setUsername("player_" + std::to_string(id));
            user->setBoostTCPSend([&api](const nx_data& data)
                                  { require(api.readMessage(data) == client::ReadResult::success,
                                            "Client rejected deathmatch message"); });
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
        auto* shopper = server::ClientStorage::getClientById(10);
        shopper->getObject3D().setPosition(Vector3f{-592.0f, 192.0f, 764.0f});
        require(room->onPlayerAction(10, 12), "Terrorist could not buy Desert Eagle at spawn");
        nx3d::economy::Snapshot purchase;
        require(nx3d::economy::decode(shooter.getChatSnapshot(777).back().message, purchase) &&
                        purchase.money == 100 && purchase.weapon == "deagle",
                "Weapon purchase did not update equipment and money");
        shopper->getObject3D().setPosition(Vector3f{0.0f, 0.0f, 0.0f});
        require(!room->onPlayerAction(10, 13), "Remote purchase was accepted");
        server::ServerConfig settings;
        server::Command command(settings);
        TestProtocol protocol;

        for (uint64_t kill = 1; kill <= 100; ++kill)
        {
            for (int shot = 0; shot < 3; ++shot)
            {
                auto packet = client::Packet::Room::Player3D::shoot(shooter, 20, 35.0f);
                nx_data payload(packet.begin() + 16, packet.end());
                require(command.read(payload, *server::ClientStorage::getClientById(10), protocol, 1) == server::CommandResult::success, "Shot rejected");
            }
            require(room->getPlayerHealth(20) == 0.0f, "Victim did not stay dead");
            require(victim.consumeRespawnEvents().empty(), "Victim respawned too early");
            Registry::update(1.9f);
            require(room->getPlayerHealth(20) == 0.0f, "Respawn delay was too short");
            Registry::update(0.2f);
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

        auto* fallingPlayer = server::ClientStorage::getClientById(20);
        fallingPlayer->getObject3D().setPosition(Vector3f{0.0f, nx3d::kFallDeathY + 1.0f, 0.0f});
        Registry::update(1.0f / 60.0f);
        require(room->getPlayerDeaths(20) == 100, "Player died above the fall plane");
        fallingPlayer->getObject3D().setPosition(Vector3f{0.0f, nx3d::kFallDeathY - 1.0f, 0.0f});
        Registry::update(1.0f / 60.0f);
        require(room->getPlayerDeaths(20) == 101, "Falling did not count as a death");
        require(room->getPlayerKills(10) == 100, "Falling credited another player with a kill");
        require(victim.consumeDamageEvents().size() == 1, "Falling did not send damage");
        require(victim.consumeRespawnEvents().empty(), "Falling respawned too early");
        Registry::update(2.1f);
        require(victim.consumeRespawnEvents().size() == 1, "Falling did not respawn the victim");

        Registry::update(1.0f / 60.0f);
        require(room->getPlayerDeaths(20) == 101, "One fall caused multiple deaths");
        fallingPlayer->getObject3D().setPosition(Vector3f{0.0f, 0.0f, 0.0f});
        Registry::update(1.0f / 60.0f);
        fallingPlayer->getObject3D().setPosition(Vector3f{0.0f, -600.0f, 0.0f});
        Registry::update(1.0f / 60.0f);
        require(room->getPlayerDeaths(20) == 102, "A later fall did not count as a death");
        nx3d::economy::Snapshot balance;
        require(nx3d::economy::decode(shooter.getChatSnapshot(777).back().message, balance) &&
                        balance.money == nx3d::economy::kMaximumMoney,
                "Kill rewards did not respect the money cap");
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
