#ifndef NX3D_CLIENT_GAMEMODE_BOMB_PLAYER_STATE_HH
#define NX3D_CLIENT_GAMEMODE_BOMB_PLAYER_STATE_HH

#include <cstdint>
#include <string_view>

namespace nx3d::client::gamemode
{

/// Before round one, an admitted player is active but has not spawned yet.
/// That is a waiting state, not an elimination.
constexpr bool shouldShowEliminated(uint64_t round, bool active, bool alive)
{
    return round > 0 && active && !alive;
}

constexpr bool isBombPlayerEliminated(uint64_t round, bool active, bool alive,
                                      bool healthDepleted)
{
    return active && (healthDepleted || shouldShowEliminated(round, active, alive));
}

/// A queued spectator can select a local weapon, but only a live player may
/// ask the server to equip it.
constexpr bool canRequestBombEquip(bool active, bool alive, std::string_view phase)
{
    return active && alive && phase == "live";
}

} // namespace nx3d::client::gamemode

#endif
