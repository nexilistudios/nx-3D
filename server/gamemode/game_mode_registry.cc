#include <server/gamemode/game_mode_registry.hh>

#include <server/gamemode/deathmatch_mode.hh>

#include <nexilis/logger/log.hh>
#include <nexilis/server/room_storage.hh>

#include <utility>

namespace nx3d::server::gamemode
{

std::unordered_map<uint64_t, std::unique_ptr<GameMode>> GameModeRegistry::m_modes;

namespace
{

std::unique_ptr<GameMode> createGameMode(nx3d::GameMode gameMode)
{
    switch (gameMode)
    {
        case nx3d::GameMode::deathmatch:
            return std::make_unique<DeathmatchMode>();

        // Not implemented yet. Fall back to deathmatch so a room requesting
        // one of these still runs (with automatic respawns) until the mode is
        // actually built.
        case nx3d::GameMode::nospawn:
        case nx3d::GameMode::hardpoint:
        case nx3d::GameMode::custom:
            nexilis::Log::warning("Gamemode not implemented, falling back to deathmatch");
            return std::make_unique<DeathmatchMode>();
    }
    return std::make_unique<DeathmatchMode>();
}

} // namespace

void GameModeRegistry::assignRoom(nexilis::server::Room& room, nx3d::GameMode gameMode)
{
    auto mode = createGameMode(gameMode);
    auto* modePtr = mode.get();

    room.setDeathHandler([modePtr](nexilis::server::Room& r, uint64_t killerId, uint64_t victimId)
                         { modePtr->onPlayerDeath(r, killerId, victimId); });

    m_modes[room.getId()] = std::move(mode);
}

void GameModeRegistry::update(float dt)
{
    for (auto& [roomId, mode] : m_modes)
    {
        if (auto* room = nexilis::server::RoomStorage::getRoomById(roomId))
            mode->update(*room, dt);
    }
}

void GameModeRegistry::clear()
{
    m_modes.clear();
}

} // namespace nx3d::server::gamemode