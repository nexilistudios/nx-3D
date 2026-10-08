#include <server/gamemode/game_mode_registry.hh>

#include <nexilis/client/client_api.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room_storage.hh>

#include <stdexcept>
#include <string>

namespace
{
void check(bool ok, const char* reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
} // namespace

int main()
{
    using namespace nexilis;
    using nx3d::server::gamemode::GameModeRegistry;
    client::ClientConfig config;
    client::ClientAPI t(config), ct(config), queued(config);
    t.setClientId(10);
    ct.setClientId(20);
    queued.setClientId(30);

    server::RoomStorage::add(server::Room(RoomData(10, "Bomb test", 777)));
    auto* room = server::RoomStorage::getRoomById(777);
    GameModeRegistry::assignRoom(*room, nx3d::GameMode::bomb);
    auto add = [&](uint64_t id, client::ClientAPI& api, const char* team)
    {
        auto user = std::make_unique<server::User>(id, "127.0.0.1");
        user->setRoomId(777);
        user->setUsername("player_" + std::to_string(id));
        user->setBoostTCPSend([&api](const nx_data& data)
                              { check(api.readMessage(data) == client::ReadResult::success, "Message rejected"); });
        server::ClientStorage::add(std::move(user));
        room->joinRoom(id);
        room->setPlayerTeam(id, team);
    };
    add(10, t, "Terrorist");
    add(20, ct, "Counter Terrorist");
    GameModeRegistry::update(1.0f);
    check(room->canDamage(10, 20), "Round failed to start");
    auto first = t.consumeMatchEvents();
    check(!first.empty() && first.back().round == 1 && first.back().alive, "Bad first round state");

    add(30, queued, "Terrorist");
    check(!room->canDamage(30, 20), "Queued player could shoot");
    auto queueEvents = queued.consumeMatchEvents();
    check(!queueEvents.empty() && queueEvents.back().queue_position == 1, "Queue position missing");

    check(room->onPlayerAction(10, 1), "Plant rejected");
    GameModeRegistry::update(3.1f);
    auto planted = t.consumeMatchEvents();
    check(!planted.empty() && planted.back().bomb_planted, "Bomb did not plant");
    check(room->onPlayerAction(20, 2), "Defuse rejected");
    GameModeRegistry::update(5.1f);
    auto defused = ct.consumeMatchEvents();
    check(!defused.empty() && defused.back().counter_terrorist_score == 1,
          "Defuse did not score");

    GameModeRegistry::update(5.1f);
    check(room->canDamage(30, 20), "Queued player not admitted next round");
    check(room->damagePlayer(20, 100.0f), "Kill did not lower health");
    room->onPlayerDied(10, 20);
    auto eliminated = t.consumeMatchEvents();
    check(!eliminated.empty() && eliminated.back().terrorist_score == 1,
          "Elimination did not score");
    check(!room->canDamage(10, 20), "Post-round shooting allowed");

    // Finish the first half, then verify the MR12 side swap and score mapping.
    for (int round = 3; round <= 12; ++round)
    {
        GameModeRegistry::update(5.1f);
        check(room->damagePlayer(20, 100.0f), "Round victim was not revived");
        room->onPlayerDied(10, 20);
    }
    GameModeRegistry::update(5.1f);
    check(room->getPlayerTeam(10) == "Counter Terrorist" &&
                  room->getPlayerTeam(20) == "Terrorist",
          "Halftime did not swap sides");
    auto secondHalf = ct.consumeMatchEvents();
    check(!secondHalf.empty() && secondHalf.back().round == 13 &&
                  secondHalf.back().counter_terrorist_score == 11,
          "Half-time score was not carried to new sides");
    check(!room->canSelectTeam(10), "Active player could switch team");

    check(room->onPlayerAction(20, 1), "Second-half plant rejected");
    GameModeRegistry::update(3.1f);
    GameModeRegistry::update(40.1f);
    auto exploded = t.consumeMatchEvents();
    check(!exploded.empty() && exploded.back().terrorist_score == 2,
          "Explosion did not score");

    GameModeRegistry::clear();
    server::RoomStorage::clear();
    server::ClientStorage::clear();
}
