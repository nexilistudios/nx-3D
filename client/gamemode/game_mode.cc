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

} // namespace nx3d::client::gamemode