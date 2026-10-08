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
    virtual void onTeamSelected(nexilis::server::Room&, uint64_t)
    {
    }
    virtual bool onAction(nexilis::server::Room&, uint64_t, uint8_t)
    {
        return false;
    }
    virtual bool canDamage(uint64_t, uint64_t) const
    {
        return true;
    }
    virtual bool canSelectTeam(uint64_t) const
    {
        return true;
    }

    /// Called once per server tick so the gamemode can run its own logic.
    virtual void update(nexilis::server::Room& room, float dt)
    {
        (void)room;
        (void)dt;
    }
};

} // namespace nx3d::server::gamemode

#endif
