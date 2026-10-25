#ifndef NX3D_SHARED_PLAYER_RULES_HH
#define NX3D_SHARED_PLAYER_RULES_HH

#include <cstdint>

namespace nx3d
{
// The generated Dust II spans about X [-2400, 2000], Z [-3300, 1400].
// Leave room for the player capsule, jumps, and the fall-death plane at -256.
inline constexpr float kWalkSpeed = 280.0f;
inline constexpr float kSilentWalkSpeed = 140.0f;
inline constexpr float kCrouchSpeed = 112.0f;
inline constexpr float kStandingEyeHeight = 64.0f;
inline constexpr float kCrouchingEyeHeight = 40.0f;
// The movement packet's Y component carries button flags; its X/Z components
// remain a unit horizontal direction. The server ignores the packet delta.
inline constexpr uint8_t kMoveJump = 1;
inline constexpr uint8_t kMoveCrouch = 2;
inline constexpr uint8_t kMoveSilentWalk = 4;
// Walking while jumping reaches about 377 units/s in three dimensions.
inline constexpr float kMaxMovementSpeed = 400.0f;
inline constexpr float kMaxHorizontalPosition = 5000.0f;
inline constexpr float kMinPlayerHeight = -512.0f;
inline constexpr float kMaxPlayerHeight = 1024.0f;

// The M16 fires at 800 RPM (13.33 shots/s); both current rifles deal 35 damage.
inline constexpr float kMaxShootingRate = 16.0f;
inline constexpr float kMaxDamagePerShot = 35.0f;
} // namespace nx3d

#endif
