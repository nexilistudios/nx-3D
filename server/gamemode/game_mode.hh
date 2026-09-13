#ifndef NX3D_SERVER_GAMEMODE_GAME_MODE_HH
#define NX3D_SERVER_GAMEMODE_GAME_MODE_HH

#include <shared/gamemode.hh>

#include <nexilis/server/room.hh>

#include <cstdint>

namespace nx3d::server::gamemode
{

/// Base class for server-side gamemodes.
///
/// A gamemode decides how a room plays: how deaths are handled (e.g. respawn
/// immediately, end the round, switch sides...), and runs optional per-tick
/// logic. Each room is assigned exactly one gamemode, identified by its
/// nx3d::GameMode type.
class GameMode
{
public:
    virtual ~GameMode() = default;

    /// The gamemode identifier of the room this mode was assigned to.
    virtual nx3d::GameMode type() const = 0;

    /// Called when a player dies in the room.
    virtual void onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId) = 0;

    /// Called once per server tick so the gamemode can run its own logic.
    virtual void update(nexilis::server::Room& room, float dt)
    {
        (void)room;
        (void)dt;
    }
};

} // namespace nx3d::server::gamemode

#endif