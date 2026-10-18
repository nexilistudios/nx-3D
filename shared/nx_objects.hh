#ifndef NX3D_SHARED_NX_OBJECTS_HH
#define NX3D_SHARED_NX_OBJECTS_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nx3d
{

enum class NXObjectKind : uint8_t { firearm, melee, throwable, bomb, equipment, consumable };

struct NXObjectVector { float x, y, z; };

struct NXObjectVisual
{
    std::string_view worldModel;
    std::string_view worldMaterial;
    std::string_view viewModel;
    std::string_view viewMaterial;
    float worldScale;
    float viewScale;
    NXObjectVector viewCenter;
    NXObjectVector viewOffset;
    NXObjectVector viewRotation;
    NXObjectVector collisionHalfExtents;
};

struct NXObjectCombat
{
    float roundsPerMinute = 0.0f;
    int magazineSize = 0;
    int reserveMagazines = 0;
    float reloadSeconds = 0.0f;
    float damage = 0.0f;
    bool automatic = false;
};

struct NXObjectEffect
{
    int maxStack = 1;
    float fuseSeconds = 0.0f;
    float durationSeconds = 0.0f;
    float radius = 0.0f;
    float maxDamage = 0.0f;
};

/// The shared item catalog. Every item has a world mesh and a first-person
/// mesh, including items applied immediately on purchase. Slot zero means the
/// item is used through the buy menu instead of equipped.
struct NXObject
{
    std::string_view id;
    std::string_view name;
    NXObjectKind kind;
    uint8_t slot;
    uint8_t buyAction;
    int price;
    std::string_view team;
    NXObjectVisual visual;
    NXObjectCombat combat;
    NXObjectEffect effect;
};

inline constexpr std::array<NXObject, 13> kNXObjects{{
    {"ak47", "AK-47", NXObjectKind::firearm, 1, 14, 2700, "Terrorist",
     {"weapons/ak47.obj", "weapons/ak47.mtl", "weapons/ak47.obj", "weapons/ak47.mtl", 7.4f, .50f, {0,-.05f,.20f}, {.68f,-.58f,2.25f}, {-10,33,-12}, {1,4.4f,11.7f}},
     {600,30,3,2.5f,35,true}},
    {"m16", "M16", NXObjectKind::firearm, 1, 15, 2900, "Counter Terrorist",
     {"weapons/m16.obj", "weapons/m16.mtl", "weapons/m16.obj", "weapons/m16.mtl", 7.3f, .50f, {0,-.05f,.30f}, {.70f,-.59f,2.30f}, {-9,35,-11}, {1,4.2f,11.7f}},
     {800,30,3,2.5f,35,true}},
    {"glock", "Glock", NXObjectKind::firearm, 2, 10, 400, "Terrorist",
     {"weapons/glock.obj", "weapons/glock.mtl", "weapons/glock.obj", "weapons/glock.mtl", 8, .70f, {0,-.15f,.10f}, {.72f,-.66f,2.05f}, {-10,33,-12}, {1,3,6}},
     {400,20,3,2.0f,24,false}},
    {"usp", "USP", NXObjectKind::firearm, 2, 11, 500, "Counter Terrorist",
     {"weapons/usp.obj", "weapons/usp.mtl", "weapons/usp.obj", "weapons/usp.mtl", 8, .70f, {0,-.15f,.10f}, {.72f,-.66f,2.05f}, {-10,33,-12}, {1,3,6}},
     {350,12,3,2.1f,30,false}},
    {"deagle", "Desert Eagle", NXObjectKind::firearm, 2, 12, 700, "",
     {"weapons/deagle.obj", "weapons/deagle.mtl", "weapons/deagle.obj", "weapons/deagle.mtl", 8, .70f, {0,-.15f,.10f}, {.72f,-.66f,2.05f}, {-10,33,-12}, {1,3,6}},
     {260,7,3,2.2f,35,false}},
    {"mp5", "MP5", NXObjectKind::firearm, 1, 13, 1500, "",
     {"weapons/mp5.obj", "weapons/mp5.mtl", "weapons/mp5.obj", "weapons/mp5.mtl", 7.8f, .56f, {0,-.08f,.20f}, {.68f,-.59f,2.22f}, {-10,33,-12}, {1,3.8f,10}},
     {800,30,3,2.4f,26,true}},
    {"knife", "Knife", NXObjectKind::melee, 3, 0, 0, "",
     {"weapons/knife.obj", "weapons/knife.mtl", "weapons/knife.obj", "weapons/knife.mtl", 8, .85f, {0,0,0}, {.7f,-.7f,1.8f}, {0,0,0}, {1,2,6}},
     {100,0,0,0,35,false}},
    {"grenade", "HE grenade", NXObjectKind::throwable, 4, 19, 300, "",
     {"weapons/grenade.obj", "weapons/grenade.mtl", "weapons/grenade.obj", "weapons/grenade.mtl", 8, .75f, {0,0,0}, {.7f,-.7f,1.8f}, {0,0,0}, {2,2,2}}, {}, {2,2,0,170,90}},
    {"smoke", "Smoke grenade", NXObjectKind::throwable, 5, 20, 300, "",
     {"weapons/grenade.obj", "weapons/grenade.mtl", "weapons/grenade.obj", "weapons/grenade.mtl", 8, .75f, {0,0,0}, {.7f,-.7f,1.8f}, {0,0,0}, {2,2,2}}, {}, {2,2,12,0,0}},
    {"bomb", "C4 bomb", NXObjectKind::bomb, 6, 0, 0, "Terrorist",
     {"bomb/c4.obj", "bomb/c4.mtl", "bomb/c4.obj", "bomb/c4.mtl", 1, .025f, {0,8.5f,0}, {.65f,-.65f,1.7f}, {0,0,0}, {19,9,20}}, {}, {1,0,40,800,200}},
    {"defuse_kit", "Defuse kit", NXObjectKind::equipment, 7, 16, 400, "Counter Terrorist",
     {"items/defuse_kit.obj", "items/defuse_kit.mtl", "items/defuse_kit.obj", "items/defuse_kit.mtl", 8, .75f, {0,0,0}, {.7f,-.7f,1.8f}, {0,0,0}, {3,2,4}}, {}},
    {"ammo_refill", "Ammo refill", NXObjectKind::consumable, 0, 17, 250, "",
     {"items/ammo_refill.obj", "items/ammo_refill.mtl", "items/ammo_refill.obj", "items/ammo_refill.mtl", 8, .75f, {0,0,0}, {.7f,-.7f,1.8f}, {0,0,0}, {3,2,4}}, {}},
    {"health_refill", "Health refill", NXObjectKind::consumable, 0, 18, 350, "",
     {"items/health_refill.obj", "items/health_refill.mtl", "items/health_refill.obj", "items/health_refill.mtl", 8, .75f, {0,0,0}, {.7f,-.7f,1.8f}, {0,0,0}, {3,2,4}}, {}},
}};

constexpr const NXObject* findNXObject(std::string_view id)
{
    for (const auto& object : kNXObjects)
        if (object.id == id)
            return &object;
    return nullptr;
}

constexpr const NXObject* findNXObjectForPurchase(uint8_t action)
{
    for (const auto& object : kNXObjects)
        if (object.buyAction != 0 && object.buyAction == action)
            return &object;
    return nullptr;
}

/// Counts of owned NXObjects. The catalog supplies each object's stack limit.
/// Firearms continue to carry their ammunition and slot data separately.
class NXObjectInventory
{
public:
    constexpr int count(std::string_view id) const
    {
        for (std::size_t i = 0; i < kNXObjects.size(); ++i)
            if (kNXObjects[i].id == id)
                return m_counts[i];
        return 0;
    }

    constexpr bool set(std::string_view id, int quantity)
    {
        for (std::size_t i = 0; i < kNXObjects.size(); ++i)
        {
            if (kNXObjects[i].id != id) continue;
            if (quantity < 0 || quantity > kNXObjects[i].effect.maxStack)
                return false;
            m_counts[i] = static_cast<uint8_t>(quantity);
            return true;
        }
        return false;
    }

    constexpr bool add(std::string_view id)
    {
        return set(id, count(id) + 1);
    }

    constexpr bool consume(std::string_view id)
    {
        return count(id) > 0 && set(id, count(id) - 1);
    }

private:
    std::array<uint8_t, kNXObjects.size()> m_counts{};
};

} // namespace nx3d

#endif
