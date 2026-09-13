#ifndef NX3D_SERVER_GAMEMODE_DEATHMATCH_MODE_HH
#define NX3D_SERVER_GAMEMODE_DEATHMATCH_MODE_HH

#include <server/gamemode/game_mode.hh>

namespace nx3d::server::gamemode
{

/// Classic deathmatch.
///
/// The victim's health is reset and they are instantly respawned by whoever
/// killed them. This is the original free-for-all behaviour of the game.
class DeathmatchMode : public GameMode
{
public:
    nx3d::GameMode type() const override;
    void onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId) override;
};

} // namespace nx3d::server::gamemode

#endif