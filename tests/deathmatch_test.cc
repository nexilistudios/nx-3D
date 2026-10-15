#include <server/gamemode/game_mode_registry.hh>
#include <shared/economy.hh>
#include <shared/fall_death.hh>
#include <shared/player_rules.hh>

#include <nexilis/client/packet.hh>
#include <nexilis/command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/room_storage.hh>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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
                            {{"action", "create"}, {"room_id", 777}, {"client_id", id}, {"room_name", "test"}, {"room_context", 1}})) == client::ReadResult::success,
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
        auto announce = [](client::ClientAPI& api, uint64_t id)
        {
            require(api.readMessage(server::Command::clientMessageData(
                            CommandType::room, "management", 0,
                            {{"action", "join"}, {"room_id", 777}, {"client_id", id}, {"username", boost::json::value("player_" + std::to_string(id))}})) ==
                            client::ReadResult::success,
                    "Test client could not register room player");
        };
        for (auto* api : {&shooter, &victim})
        {
            announce(*api, 10);
            announce(*api, 20);
        }

        // Register callbacks before adding more rooms, as the real server does.
        for (uint64_t id = 777; id < 780; ++id)
        {
            server::RoomStorage::add(server::Room(RoomData(10, "test", id)));
            Registry::assignRoom(*server::RoomStorage::getRoomById(id), nx3d::GameMode::deathmatch);
        }
        auto* room = server::RoomStorage::getRoomById(777);
        room->joinRoom(10);
        room->joinRoom(20);
        server::ServerConfig settings;
        // This fixture fires hundreds of shots back-to-back to exercise
        // death/respawn; production keeps its separate fire-rate limit.
        settings.getGameplayLimits().setMaxShootingRate(0.0f);
        server::Command command(settings);
        TestProtocol protocol;
        auto sendAsShooter = [&](const nx_data& packet)
        {
            nx_data payload(packet.begin() + 16, packet.end());
            return command.read(payload, *server::ClientStorage::getClientById(10), protocol, 1);
        };
        const auto beforeTeam = server::ClientStorage::getClientById(10)->getObject3D().getPosition();
        require(sendAsShooter(client::Packet::Room::Player3D::movement(
                        shooter, Vector3f{1.0f, 0.0f, 0.0f}, 0.0f, 0.0f)) ==
                        server::CommandResult::success,
                "Movement before team selection was reported as invalid");
        require(server::ClientStorage::getClientById(10)->getObject3D().getPosition().x == beforeTeam.x,
                "Movement before team selection changed server position");
        require(sendAsShooter(client::Packet::Room::Player3D::movement(
                        shooter, Vector3f{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f},
                        0.0f, 0.0f)) == server::CommandResult::invalid_input,
                "Non-finite movement was accepted before team selection");
        room->setPlayerTeam(10, "Terrorist");
        room->setPlayerTeam(20, "Counter Terrorist");
        auto* shopper = server::ClientStorage::getClientById(10);
        shopper->getObject3D().setPosition(Vector3f{-592.0f, 192.0f, 764.0f});
        require(room->onPlayerAction(10, 12), "Terrorist could not buy Desert Eagle at spawn");
        nx3d::economy::Snapshot purchase;
        require(nx3d::economy::decode(shooter.getChatSnapshot(777).back().message, purchase) &&
                        purchase.money == 100 && purchase.weapon == "deagle",
                "Weapon purchase did not update equipment and money");
        require(sendAsShooter(client::Packet::Room::Player3D::matchAction(
                        shooter, nx3d::economy::kEquipSecondaryAction)) ==
                        server::CommandResult::success,
                "Server rejected the owned equipped weapon");
        {
            const auto events = victim.consumeAnimationEvents();
            require(std::any_of(events.begin(), events.end(),
                                [](const auto& event)
                                { return event.client_id == 10 && event.state == "equipped" &&
                                         event.weapon == "deagle"; }),
                    "Server did not broadcast the equipped weapon");
        }
        shopper->getObject3D().setPosition(Vector3f{0.0f, 0.0f, 0.0f});
        require(!room->onPlayerAction(10, 13), "Remote purchase was accepted");
        const auto originalPosition = shopper->getObject3D().getPosition();
        require(sendAsShooter(client::Packet::Room::Player3D::position(
                        shooter, Vector3f{4000.0f, 400.0f, 4000.0f})) ==
                        server::CommandResult::invalid_input,
                "Client position packet bypassed server movement");
        require(shopper->getObject3D().getPosition().x == originalPosition.x,
                "Rejected position changed server state");
        require(sendAsShooter(client::Packet::Room::Player3D::shoot(shooter, 20, 35.0f)) ==
                        server::CommandResult::invalid_input,
                "Client target/damage packet bypassed server hit test");
        require(room->getPlayerHealth(20) == 100.0f, "Spoofed damage was accepted");
        require(sendAsShooter(client::Packet::Room::Player3D::fire(
                        shooter, Vector3f{0.0f, 0.0f, -1.0f}, 1)) ==
                        server::CommandResult::success,
                "Unowned weapon request was not handled");
        require(room->getPlayerHealth(20) == 100.0f, "Unowned weapon dealt damage");
        require(sendAsShooter(client::Packet::Room::Player3D::fire(
                        shooter, Vector3f{0.0f, 1.0f, 0.0f}, 2)) ==
                        server::CommandResult::success,
                "Owned weapon shot was rejected");
        require(sendAsShooter(client::Packet::Room::Player3D::reload(shooter, 2)) ==
                        server::CommandResult::success,
                "Reload request was rejected");
        {
            const auto events = victim.consumeAnimationEvents();
            require(std::any_of(events.begin(), events.end(),
                                [](const auto& event)
                                { return event.client_id == 10 && event.state == "reloading" &&
                                         event.weapon == "deagle" && event.duration > 2.0f; }),
                    "Server did not broadcast the accepted reload animation");
            const auto ownEvents = shooter.consumeAnimationEvents();
            require(std::any_of(ownEvents.begin(), ownEvents.end(),
                                [](const auto& event)
                                { return event.client_id == 10 && event.state == "reloading" &&
                                         event.weapon == "deagle"; }),
                    "Server did not confirm reload to the shooter");
        }

        // Client-supplied displacement and delta time are ignored in this room.
        require(sendAsShooter(client::Packet::Room::Player3D::movement(
                        shooter, Vector3f{100000.0f, 100000.0f, 0.0f}, 1000.0f, 0.0f)) ==
                        server::CommandResult::success,
                "Movement intent rejected");
        const auto afterMove = shopper->getObject3D().getPosition();
        require(std::abs(afterMove.x - originalPosition.x) <= nx3d::kWalkSpeed * 0.1f + 1.0f,
                "Forged movement speed or delta moved the server player too far");

        const auto groundItem = client::Packet::Room::GameItem::create(
                shooter, originalPosition, Vector3f{1, 1, 1},
                "ak47:30:90", "on_ground", "");
        require(sendAsShooter(groundItem) == server::CommandResult::failure,
                "Client created a weapon pickup");
        require(sendAsShooter(client::Packet::Room::Player3D::matchAction(
                        shooter, nx3d::economy::kDropSecondaryAction)) ==
                        server::CommandResult::success,
                "Server rejected a legitimate weapon drop");
        require(room->getGameItems().size() == 1, "Server did not create the pickup");
        const auto itemId = room->getGameItems().front().getId();
        require(sendAsShooter(client::Packet::Room::GameItem::destroy(shooter, itemId)) ==
                        server::CommandResult::failure,
                "Client removed a server weapon pickup");
        require(sendAsShooter(client::Packet::Room::GameItem::update(
                        shooter, itemId, "on_ground")) == server::CommandResult::failure,
                "Client changed a weapon status without picking it up");
        const auto pickupPosition = shopper->getObject3D().getPosition();
        shopper->getObject3D().setPosition(Vector3f{4000, 100, 4000});
        require(sendAsShooter(client::Packet::Room::GameItem::update(
                        shooter, itemId, "picked_up")) == server::CommandResult::failure,
                "Client picked up a distant weapon");
        shopper->getObject3D().setPosition(pickupPosition);
        require(sendAsShooter(client::Packet::Room::GameItem::update(
                        shooter, itemId, "picked_up")) == server::CommandResult::success,
                "Server rejected a nearby weapon pickup");
        require(room->getGameItems().empty(),
                "Server did not consume the pickup");
        require(sendAsShooter(client::Packet::Room::GameItem::update(
                        shooter, itemId, "picked_up")) == server::CommandResult::failure,
                "Server allowed the same weapon to be picked up twice");

        room->spawnPlayer(10);
        shopper->getObject3D().setPosition(Vector3f{pickupPosition.x,
                                                    pickupPosition.y + 100.0f,
                                                    pickupPosition.z});
        const float airborneY = shopper->getObject3D().getPosition().y;
        Registry::update(0.05f);
        require(shopper->getObject3D().getPosition().y < airborneY,
                "Server stopped gravity when movement packets stopped");

        room->spawnPlayer(10);
        const auto standingPosition = shopper->getObject3D().getPosition();
        require(sendAsShooter(client::Packet::Room::Player3D::movement(
                        shooter, Vector3f{1.0f, static_cast<float>(nx3d::kMoveSilentWalk), 0.0f},
                        0.0f, 0.0f)) == server::CommandResult::success,
                "Silent walk input was rejected");
        Registry::update(0.05f);
        require(shopper->getObject3D().getPosition().x - standingPosition.x <=
                        nx3d::kSilentWalkSpeed * 0.05f + 0.1f,
                "Silent walk exceeded its server speed limit");
        auto animationEvents = victim.consumeAnimationEvents();
        require(std::any_of(animationEvents.begin(), animationEvents.end(),
                            [](const auto& event)
                            { return event.client_id == 10 && event.state == "sneaking"; }),
                "Server did not broadcast the sneaking animation");
        const auto beforeCrouch = shopper->getObject3D().getPosition();
        require(sendAsShooter(client::Packet::Room::Player3D::movement(
                        shooter, Vector3f{0.0f, static_cast<float>(nx3d::kMoveCrouch), 0.0f},
                        0.0f, 0.0f)) == server::CommandResult::success,
                "Crouch input was rejected");
        Registry::update(0.05f);
        animationEvents = victim.consumeAnimationEvents();
        require(std::any_of(animationEvents.begin(), animationEvents.end(),
                            [](const auto& event)
                            { return event.client_id == 10 && event.state == "crouching"; }),
                "Server did not broadcast the crouching animation");
        require(shopper->getObject3D().getDimensions().y == nx3d::kCrouchingEyeHeight &&
                        shopper->getObject3D().getPosition().y < beforeCrouch.y - 20.0f,
                "Server did not lower the crouched player");
        const auto crouchedSnapshots = victim.getRemotePlayersSnapshot(777, 20);
        const auto crouchedRemote = std::find_if(crouchedSnapshots.begin(), crouchedSnapshots.end(),
                                               [](const auto& player) { return player.id == 10; });
        require(crouchedRemote != crouchedSnapshots.end() &&
                        crouchedRemote->h == nx3d::kCrouchingEyeHeight,
                "Crouch stance was not replicated to other clients");
        require(sendAsShooter(client::Packet::Room::Player3D::movement(
                        shooter, Vector3f{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f)) ==
                        server::CommandResult::success,
                "Stand input was rejected");
        Registry::update(0.05f);
        require(shopper->getObject3D().getDimensions().y == nx3d::kStandingEyeHeight,
                "Server did not restore standing height");
        require(sendAsShooter(client::Packet::Room::Player3D::movement(
                        shooter, Vector3f{1.0f, 0.0f, 0.0f}, 0.0f, 0.0f)) ==
                        server::CommandResult::success,
                "Running input was rejected");
        Registry::update(0.05f);
        animationEvents = victim.consumeAnimationEvents();
        require(std::any_of(animationEvents.begin(), animationEvents.end(),
                            [](const auto& event)
                            { return event.client_id == 10 && event.state == "running"; }),
                "Server did not broadcast the running animation");

        for (uint64_t kill = 1; kill <= 100; ++kill)
        {
            auto* target = server::ClientStorage::getClientById(20);
            shopper->getObject3D().setPosition(Vector3f{-592.0f, 192.0f, 764.0f});
            target->getObject3D().setPosition(Vector3f{-592.0f, 192.0f, 764.0f});
            for (int shot = 0; shot < 3; ++shot)
            {
                require(sendAsShooter(client::Packet::Room::Player3D::fire(
                                shooter, Vector3f{0.0f, 0.0f, -1.0f}, 3)) ==
                                server::CommandResult::success,
                        "Shot rejected");
            }
            if (kill == 1)
            {
                animationEvents = victim.consumeAnimationEvents();
                require(std::any_of(animationEvents.begin(), animationEvents.end(),
                                    [](const auto& event)
                                    { return event.client_id == 10 && event.state == "shooting" &&
                                             event.weapon == "knife"; }),
                        "Server did not broadcast the shooting animation");
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
