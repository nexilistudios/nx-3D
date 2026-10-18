#ifndef NX3D_SHARED_ECONOMY_HH
#define NX3D_SHARED_ECONOMY_HH

#include <shared/nx_objects.hh>

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
    const NXObject* object;
};

constexpr ItemKind itemKind(const NXObject& object)
{
    if (object.kind == NXObjectKind::firearm) return ItemKind::weapon;
    if (object.kind == NXObjectKind::throwable) return ItemKind::grenade;
    if (object.id == "defuse_kit") return ItemKind::defuseKit;
    if (object.id == "ammo_refill") return ItemKind::ammo;
    return ItemKind::health;
}

inline constexpr auto kItems = []
{
    std::array<Item, 11> items{};
    for (uint8_t action = 10; action <= 20; ++action)
    {
        const auto* object = findNXObjectForPurchase(action);
        items[action - 10] = {action, object->name.data(),
                              object->kind == NXObjectKind::firearm ? object->id.data() : "",
                              object->price, itemKind(*object), object->team.data(), object};
    }
    return items;
}();
inline constexpr uint8_t kThrowGrenadeAction = 30;
inline constexpr uint8_t kDropPrimaryAction = 31;
inline constexpr uint8_t kDropSecondaryAction = 32;
inline constexpr uint8_t kThrowSmokeAction = 33;
inline constexpr uint8_t kDropBombAction = 34;
// Client requests a visible slot change; the server resolves the weapon from
// its inventory before broadcasting it to other players.
inline constexpr uint8_t kEquipPrimaryAction = 40;
inline constexpr uint8_t kEquipSecondaryAction = 41;
inline constexpr uint8_t kEquipKnifeAction = 42;
inline constexpr uint8_t kEquipGrenadeAction = 43;
inline constexpr uint8_t kEquipSmokeAction = 44;
inline constexpr uint8_t kEquipBombAction = 45;
inline constexpr uint8_t kEquipDefuseKitAction = 46;
inline constexpr int kMaxGrenades = findNXObject("grenade")->effect.maxStack;
inline constexpr int kMaxSmokes = findNXObject("smoke")->effect.maxStack;
inline constexpr float kGrenadeFuseSeconds = findNXObject("grenade")->effect.fuseSeconds;
inline constexpr float kSmokeDurationSeconds = findNXObject("smoke")->effect.durationSeconds;
inline constexpr float kGrenadeThrowDistance = 180.0f;
inline constexpr float kGrenadeBlastRadius = findNXObject("grenade")->effect.radius;
inline constexpr float kGrenadeMaxDamage = findNXObject("grenade")->effect.maxDamage;


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
    const auto* object = findNXObject(weapon);
    return object && object->slot == 2;
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
    NXObjectInventory objects;
    std::string effect = "none";
    std::string notice;
    std::string primary;
    std::string secondary;
};

inline constexpr std::string_view kMessagePrefix = "\x1eNX3D_ECONOMY ";
inline constexpr std::string_view kGrenadeMessagePrefix = "\x1eNX3D_GRENADE ";

struct GrenadeThrow
{
    uint64_t owner = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float endX = 0.0f, endZ = 0.0f;
    std::string objectId = "grenade";
};

inline std::string encodeGrenade(const GrenadeThrow& throwEvent)
{
    return std::string(kGrenadeMessagePrefix) + std::to_string(throwEvent.owner) + " " +
           std::to_string(throwEvent.x) + " " + std::to_string(throwEvent.y) + " " +
           std::to_string(throwEvent.z) + " " + std::to_string(throwEvent.endX) + " " +
           std::to_string(throwEvent.endZ) + " " + throwEvent.objectId;
}

inline bool decodeGrenade(std::string_view message, GrenadeThrow& throwEvent)
{
    if (!message.starts_with(kGrenadeMessagePrefix))
        return false;
    std::istringstream input{std::string(message.substr(kGrenadeMessagePrefix.size()))};
    GrenadeThrow next;
    if (!(input >> next.owner >> next.x >> next.y >> next.z >> next.endX >> next.endZ >> next.objectId) ||
        next.owner == 0 || !std::isfinite(next.x) || !std::isfinite(next.y) ||
        !std::isfinite(next.z) || !std::isfinite(next.endX) || !std::isfinite(next.endZ) ||
        !findNXObject(next.objectId) ||
        findNXObject(next.objectId)->kind != NXObjectKind::throwable)
        return false;
    throwEvent = next;
    return true;
}

inline std::string encode(const Snapshot& state)
{
    return std::string(kMessagePrefix) + std::to_string(state.money) + " " +
           state.weapon + " " + std::to_string(state.objects.count("defuse_kit")) + " " +
           state.effect + " " + (state.primary.empty() ? "-" : state.primary) + " " +
           (state.secondary.empty() ? "-" : state.secondary) + " " +
           std::to_string(state.objects.count("grenade")) + " " +
           std::to_string(state.objects.count("smoke")) + " " +
           std::to_string(state.objects.count("bomb")) + " " + state.notice;
}

inline bool decode(std::string_view message, Snapshot& state)
{
    if (!message.starts_with(kMessagePrefix))
        return false;
    std::istringstream input{std::string(message.substr(kMessagePrefix.size()))};
    Snapshot next;
    int kit = 0, bomb = 0, grenades = 0, smokes = 0;
    if (!(input >> next.money >> next.weapon >> kit >> next.effect >>
          next.primary >> next.secondary >> grenades >> smokes >> bomb) ||
        next.money < 0 || next.money > kMaximumMoney || (kit != 0 && kit != 1) ||
        (bomb != 0 && bomb != 1) ||
        !next.objects.set("defuse_kit", kit) ||
        !next.objects.set("grenade", grenades) ||
        !next.objects.set("smoke", smokes) ||
        !next.objects.set("bomb", bomb))
        return false;
    if (next.primary == "-") next.primary.clear();
    if (next.secondary == "-") next.secondary.clear();
    std::getline(input >> std::ws, next.notice);
    state = std::move(next);
    return true;
}

} // namespace nx3d::economy

#endif
