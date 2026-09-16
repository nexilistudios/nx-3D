#include <client/gamemode/game_mode.hh>

#include <client/app/client_app.hh>

namespace nx3d::client::gamemode
{

/// Frames to wait after a hit before playing the hitmarker sound, so it is
/// not masked by the gunshot's initial attack.
namespace
{
constexpr int kHitmarkerDelayFrames = 10;
}

void GameMode::updateHitmarkerSound(ClientApp& app)
{
    if (m_hitmarkerAudioDelay > 0)
    {
        m_hitmarkerAudioDelay--;
        if (m_hitmarkerAudioDelay == 0 && app.hitmarker_audio)
            app.hitmarker_audio->play();
    }
}

void GameMode::triggerHitmarkerSound()
{
    m_hitmarkerAudioDelay = kHitmarkerDelayFrames;
}

void GameMode::updateWalkSound(ClientApp& app, float horizontalSpeed)
{
    if (!app.walk_audio)
        return;

    // 40.0f engine units/s: full walk is ~250, so this triggers on real
    // movement while staying quiet against near-zero camera jitter.
    constexpr float kMinWalkSpeed = 40.0f;
    const bool moving = horizontalSpeed >= kMinWalkSpeed;

    if (moving)
    {
        if (!app.walk_audio->isPlaying())
            app.walk_audio->play();
    }
    else if (app.walk_audio->isPlaying())
    {
        app.walk_audio->stop();
    }
}

} // namespace nx3d::client::gamemode