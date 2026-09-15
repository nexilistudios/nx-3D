#ifndef NX3D_CLIENT_GAMEMODE_GAME_MODE_HH
#define NX3D_CLIENT_GAMEMODE_GAME_MODE_HH

#include <shared/gamemode.hh>

#include <SDL3/SDL_events.h>

#include <string>

namespace nx3d::client
{
class ClientApp;
}

namespace nx3d::client::gamemode
{

/// Base class for client-side gamemodes.
///
/// A gamemode controls everything that happens once a player is in the game:
/// which team the player joins, where they spawn, which weapons they carry,
/// how kills/respawns are communicated and how the game is won or lost.
/// The server decides which gamemode a room runs; the client picks the matching
/// implementation through its type().
class GameMode
{
public:
    virtual ~GameMode() = default;

    /// Identifier that must match the gamemode the server assigned to the room.
    virtual nx3d::GameMode type() const = 0;

    /// Called once when the player enters the game after choosing a team.
    virtual void onEnter(ClientApp& app, const std::string& team) = 0;

    /// Called every frame while the app is in the Game state.
    virtual void update(ClientApp& app, float delta_time) = 0;

    /// Called for mouse button presses while the app is in the Game state.
    virtual void handleMouseButtonDown(ClientApp& app, const SDL_Event& event) = 0;

    /// Called for key presses while the app is in the Game state.
    virtual void handleKeyDown(ClientApp& app, const SDL_Event& event) = 0;

    /// Called for key releases while the app is in the Game state. Defaults to
    /// doing nothing; gamemodes override it to handle hold-actions like Tab.
    virtual void handleKeyUp(ClientApp&, const SDL_Event&)
    {
    }

protected:
    /// Advance and play the shared hitmarker sound. Gamemodes should call this
    /// from their update() so the hitmarker audio works regardless of mode.
    void updateHitmarkerSound(ClientApp& app);

    /// Request the hitmarker sound to play shortly after a hit. Call whenever
    /// a shot fired by the local player lands on another player.
    void triggerHitmarkerSound();

private:
    /// Frames until the delayed hitmarker sound should play (lets the
    /// gunshot's initial attack pass so the hitmarker is audible).
    int m_hitmarkerAudioDelay = 0;
};

} // namespace nx3d::client::gamemode

#endif