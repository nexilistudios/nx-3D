#ifndef NX3D_CLIENT_AUDIO_VOICE_PACK_HH
#define NX3D_CLIENT_AUDIO_VOICE_PACK_HH

#include <array>
#include <cstddef>

namespace nx3d::client::audio
{
enum class VoiceCue : std::size_t
{
    terroristsWin,
    counterTerroristsWin,
    hurt,
    bombPlanted,
    bombDefused,
    count
};

inline constexpr std::array<const char*, static_cast<std::size_t>(VoiceCue::count)> kVoiceCueFiles{{
        "terrorists_win.wav", "counter_terrorists_win.wav", "hurt.wav",
        "bomb_planted.wav", "bomb_defused.wav"}};

struct VoicePackDefinition
{
    const char* id;
    const char* name;
};

// Add a directory under assets/sounds/voicepacks with every cue above, then
// register its id and display name here. Existing saved ids stay stable.
inline constexpr std::array<VoicePackDefinition, 2> kVoicePacks{{
        {"default", "DEFAULT"},
        {"alternative", "ALTERNATIVE"}}};
} // namespace nx3d::client::audio

#endif
