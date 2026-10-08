#include <client/gamemode/game_mode.hh>

#include <client/app/client_app.hh>

#include <nexilis/client/packet.hh>

namespace nx3d::client::gamemode
{

/// Frames to wait after a hit before playing the hitmarker sound, so it is
/// not masked by the gunshot's initial attack.
namespace
{
constexpr int kHitmarkerDelayFrames = 10;
/// Engine units walked between consecutive positional footstep events
/// (~a natural walk rhythm at the player's walk speed).
constexpr float kStepInterval = 130.0f;
} // namespace

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

void GameMode::updateWalkSound(ClientApp& app, float horizontalSpeed, float delta_time)
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

    // Broadcast positional footstep events so other clients can spatialize
    // where the local player is walking. Our own steps stay centered (the
    // loop above), so the server simply relays the event to everyone.
    if (moving && app.ready && app.client_api.clientInRoom())
    {
        m_stepDistanceSinceLastEvent += horizontalSpeed * std::max(delta_time, 0.0f);
        if (m_stepDistanceSinceLastEvent >= kStepInterval)
        {
            m_stepDistanceSinceLastEvent = 0.0f;
            glm::vec3 pos = app.camera.getPosition();
            app.tcp_client.sendMessage(nexilis::client::Packet::Room::Player3D::audioEvent(
                    app.client_api, audio::kSoundFootstep,
                    nexilis::Vector3f({pos.x, pos.y, pos.z})));
        }
    }
    else
    {
        m_stepDistanceSinceLastEvent = 0.0f;
    }
}

void GameMode::updateRemoteAudioEvents(ClientApp& app)
{
    auto events = app.client_api.consumeAudioEvents();
    for (auto& evt : events)
    {
        // Never spatialize our own sounds back at ourselves; they are already
        // played centered by the local code that triggered them.
        if (evt.client_id == app.client_api.getClientId())
            continue;

        glm::vec3 pos(evt.x, evt.y, evt.z);
        if (evt.sound == audio::kSoundShoot)
        {
            if (app.gunshot_audio)
                app.gunshot_audio->playAt(pos);
        }
        else if (evt.sound == audio::kSoundFootstep)
        {
            if (app.step_audio)
                app.step_audio->playAt(pos);
        }
        else if (evt.sound == audio::kSoundReload)
        {
            if (app.reload_audio)
                app.reload_audio->playAt(pos);
        }
        else if (evt.sound == audio::kSoundPlanting)
        {
            if (app.planting_audio)
                app.planting_audio->playAt(pos);
        }
    }
}

} // namespace nx3d::client::gamemode
