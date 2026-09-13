#ifndef NX3D_SERVER_GAMEMODE_GAME_MODE_REGISTRY_HH
#define NX3D_SERVER_GAMEMODE_GAME_MODE_REGISTRY_HH

#include <server/gamemode/game_mode.hh>

#include <nexilis/server/room.hh>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace nx3d::server::gamemode
{

/// Owns the gamemode of every room and wires it up to the room's death events.
///
/// This is where the gamemode lives: nexilis rooms do not carry a gamemode, so
/// the registry is the single game-side source of truth for which room runs
/// which mode. Assigning a gamemode to a room also sets up the room's death
/// handler so that the room automatically forwards deaths to the gamemode.
class GameModeRegistry
{
public:
    /// Attach a gamemode to a room and install its death handler.
    static void assignRoom(nexilis::server::Room& room, nx3d::GameMode gameMode);

    /// Run every gamemode's per-tick update logic.
    static void update(float dt);

    /// Remove all gamemodes.
    static void clear();

private:
    static std::unordered_map<uint64_t, std::unique_ptr<GameMode>> m_modes;
};

} // namespace nx3d::server::gamemode

#endif