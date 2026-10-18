#ifndef NX3D_CLIENT_GAMEMODE_WEAPON_PROFILES_HH
#define NX3D_CLIENT_GAMEMODE_WEAPON_PROFILES_HH

#include <client/project_root.hh>
#include <shared/nx_objects.hh>

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>

#include <string>

namespace nx3d::client::gamemode
{

/// Renderer-friendly view of the shared NXObject definition.
struct WeaponProfile
{
    std::string name, type, obj_path, mtl_path, view_obj_path, view_mtl_path;
    float ground_scale = 1.0f, fp_scale = 1.0f;
    glm::vec3 fp_center{0.0f}, fp_offset{0.0f}, fp_rotation{0.0f};
    btVector3 half_extents{1.0f, 1.0f, 1.0f};
    float rounds_per_minute = 0.0f;
    int magazine_size = 0, reserve_magazines = 0;
    float reload_seconds = 0.0f, damage = 0.0f;
    bool automatic = false;
};

inline bool hasWeaponProfile(const std::string& type)
{
    const auto* object = findNXObject(type);
    return object && object->kind == NXObjectKind::firearm;
}

inline WeaponProfile getWeaponProfile(const std::string& type)
{
    const auto* object = findNXObject(type);
    if (!object)
        object = findNXObject("ak47");
    const auto& v = object->visual;
    const auto& c = object->combat;
    return {std::string(object->name), std::string(object->id),
            nx3d::projectAssetPath(std::string(v.worldModel)),
            nx3d::projectAssetPath(std::string(v.worldMaterial)),
            nx3d::projectAssetPath(std::string(v.viewModel)),
            nx3d::projectAssetPath(std::string(v.viewMaterial)),
            v.worldScale, v.viewScale,
            {v.viewCenter.x, v.viewCenter.y, v.viewCenter.z},
            {v.viewOffset.x, v.viewOffset.y, v.viewOffset.z},
            {v.viewRotation.x, v.viewRotation.y, v.viewRotation.z},
            {v.collisionHalfExtents.x, v.collisionHalfExtents.y, v.collisionHalfExtents.z},
            c.roundsPerMinute, c.magazineSize, c.reserveMagazines,
            c.reloadSeconds, c.damage, c.automatic};
}

} // namespace nx3d::client::gamemode

#endif
