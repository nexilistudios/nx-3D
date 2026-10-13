#ifndef NX3D_SHARED_ECONOMY_HH
#define NX3D_SHARED_ECONOMY_HH

#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace nx3d::economy
{

inline constexpr int kStartingMoney = 800;
inline constexpr int kMaximumMoney = 16000;
inline constexpr int kKillReward = 300;
inline constexpr int kRoundWinReward = 3250;
inline constexpr int kRoundLossReward = 1400;
inline constexpr int kPlantReward = 800;
inline constexpr int kDefuseReward = 800;
inline constexpr float kBuyRadius = 260.0f;
inline constexpr float kBuyHeight = 140.0f;

enum class ItemKind : uint8_t { weapon, defuseKit, ammo, health, grenade };

struct Item
{
    uint8_t action;
    const char* name;
    const char* weapon;
    int price;
    ItemKind kind;
    const char* team; // Empty means both teams.
};

inline constexpr std::array<Item, 11> kItems{{
        {10, "Glock", "glock", 400, ItemKind::weapon, "Terrorist"},
        {11, "USP", "usp", 500, ItemKind::weapon, "Counter Terrorist"},
        {12, "Desert Eagle", "deagle", 700, ItemKind::weapon, ""},
        {13, "MP5", "mp5", 1500, ItemKind::weapon, ""},
        {14, "AK-47", "ak47", 2700, ItemKind::weapon, "Terrorist"},
        {15, "M16", "m16", 2900, ItemKind::weapon, "Counter Terrorist"},
        {16, "Defuse kit", "", 400, ItemKind::defuseKit, "Counter Terrorist"},
        {17, "Ammo refill", "", 250, ItemKind::ammo, ""},
        {18, "Health refill", "", 350, ItemKind::health, ""},
        {19, "HE grenade", "", 300, ItemKind::grenade, ""},
        {20, "Smoke grenade", "", 300, ItemKind::grenade, ""},
}};
inline constexpr uint8_t kThrowGrenadeAction = 30;
inline constexpr uint8_t kDropPrimaryAction = 31;
inline constexpr uint8_t kDropSecondaryAction = 32;
inline constexpr uint8_t kThrowSmokeAction = 33;
inline constexpr int kMaxGrenades = 2;
inline constexpr int kMaxSmokes = 2;
inline constexpr float kGrenadeFuseSeconds = 2.0f;
inline constexpr float kSmokeDurationSeconds = 12.0f;
inline constexpr float kGrenadeThrowDistance = 180.0f;
inline constexpr float kGrenadeBlastRadius = 170.0f;
inline constexpr float kGrenadeMaxDamage = 90.0f;


inline const Item* itemForAction(uint8_t action)
{
    for (const auto& item : kItems)
        if (item.action == action)
            return &item;
    return nullptr;
}

inline std::string_view defaultWeapon(std::string_view team)
{
    return team == "Counter Terrorist" ? "usp" : "glock";
}

inline bool isSecondary(std::string_view weapon)
{
    return weapon == "glock" || weapon == "usp" || weapon == "deagle";
}

struct SpawnPoint { float x, y, z; };
inline constexpr std::array<SpawnPoint, 5> kCtSpawns{{
        {355.0f, -48.0f, -2364.0f}, {178.0f, -48.0f, -2408.0f},
        {381.0f, -48.0f, -2470.0f}, {180.0f, -48.0f, -2368.0f},
        {123.0f, -48.0f, -2519.0f},
}};
inline constexpr std::array<SpawnPoint, 5> kTSpawns{{
        {-592.0f, 192.0f, 764.0f}, {-834.0f, 192.0f, 797.0f},
        {-561.0f, 192.0f, 771.0f}, {-801.0f, 192.0f, 777.0f},
        {-705.0f, 204.0f, 947.0f},
}};

inline bool inBuyZone(std::string_view team, float x, float y, float z)
{
    auto close = [=](const auto& spawns)
    {
        for (const auto& spawn : spawns)
        {
            const float dx = x - spawn.x, dz = z - spawn.z;
            if (dx * dx + dz * dz <= kBuyRadius * kBuyRadius &&
                y >= spawn.y - kBuyHeight && y <= spawn.y + kBuyHeight)
                return true;
        }
        return false;
    };
    if (team == "Counter Terrorist")
        return close(kCtSpawns);
    if (team == "Terrorist")
        return close(kTSpawns);
    return false;
}

struct Snapshot
{
    int money = kStartingMoney;
    std::string weapon;
    bool hasDefuseKit = false;
    std::string effect = "none";
    std::string notice;
    std::string primary;
    std::string secondary;
    int grenades = 0;
    int smokes = 0;
};

inline constexpr std::string_view kMessagePrefix = "\x1eNX3D_ECONOMY ";
inline constexpr std::string_view kGrenadeMessagePrefix = "\x1eNX3D_GRENADE ";

struct GrenadeThrow
{
    uint64_t owner = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float endX = 0.0f, endZ = 0.0f;
    bool smoke = false;
};

inline std::string encodeGrenade(const GrenadeThrow& throwEvent)
{
    return std::string(kGrenadeMessagePrefix) + std::to_string(throwEvent.owner) + " " +
           std::to_string(throwEvent.x) + " " + std::to_string(throwEvent.y) + " " +
           std::to_string(throwEvent.z) + " " + std::to_string(throwEvent.endX) + " " +
           std::to_string(throwEvent.endZ) + " " + (throwEvent.smoke ? "1" : "0");
}

inline bool decodeGrenade(std::string_view message, GrenadeThrow& throwEvent)
{
    if (!message.starts_with(kGrenadeMessagePrefix))
        return false;
    std::istringstream input{std::string(message.substr(kGrenadeMessagePrefix.size()))};
    GrenadeThrow next;
    int smoke = 0;
    if (!(input >> next.owner >> next.x >> next.y >> next.z >> next.endX >> next.endZ >> smoke) ||
        (smoke != 0 && smoke != 1) ||
        next.owner == 0 || !std::isfinite(next.x) || !std::isfinite(next.y) ||
        !std::isfinite(next.z) || !std::isfinite(next.endX) || !std::isfinite(next.endZ))
        return false;
    next.smoke = smoke != 0;
    throwEvent = next;
    return true;
}

inline std::string encode(const Snapshot& state)
{
    return std::string(kMessagePrefix) + std::to_string(state.money) + " " +
           state.weapon + " " + (state.hasDefuseKit ? "1" : "0") + " " +
           state.effect + " " + (state.primary.empty() ? "-" : state.primary) + " " +
           (state.secondary.empty() ? "-" : state.secondary) + " " +
           std::to_string(state.grenades) + " " + std::to_string(state.smokes) + " " + state.notice;
}

inline bool decode(std::string_view message, Snapshot& state)
{
    if (!message.starts_with(kMessagePrefix))
        return false;
    std::istringstream input{std::string(message.substr(kMessagePrefix.size()))};
    Snapshot next;
    int kit = 0;
    if (!(input >> next.money >> next.weapon >> kit >> next.effect >>
          next.primary >> next.secondary >> next.grenades >> next.smokes) ||
        next.money < 0 || next.money > kMaximumMoney || (kit != 0 && kit != 1) ||
        next.grenades < 0 || next.grenades > kMaxGrenades ||
        next.smokes < 0 || next.smokes > kMaxSmokes)
        return false;
    if (next.primary == "-") next.primary.clear();
    if (next.secondary == "-") next.secondary.clear();
    next.hasDefuseKit = kit == 1;
    std::getline(input >> std::ws, next.notice);
    state = std::move(next);
    return true;
}

} // namespace nx3d::economy

#endif
