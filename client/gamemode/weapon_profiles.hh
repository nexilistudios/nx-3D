#ifndef NX3D_CLIENT_GAMEMODE_WEAPON_PROFILES_HH
#define NX3D_CLIENT_GAMEMODE_WEAPON_PROFILES_HH

#include <client/project_root.hh>

#include <btBulletDynamicsCommon.h>

#include <glm/glm.hpp>

#include <string>

namespace nx3d::client::gamemode
{

/// Rendering/physics profile of a weapon that can be held in first person or
/// dropped as a physics-driven pickup on the ground.
struct WeaponProfile
{
    std::string name;
    std::string type;
    std::string obj_path;
    std::string mtl_path;
    float ground_scale = 1.0f;
    float fp_scale = 1.0f;
    glm::vec3 fp_center{0.0f};
    btVector3 half_extents{1.0f, 1.0f, 1.0f};
};

/// Whether a weapon profile is known for the given item type.
inline bool hasWeaponProfile(const std::string& type)
{
    return type == "ak47" || type == "m4";
}

/// Get the profile for a weapon type ("ak47" / "m4"). Unknown types fall back
/// to the AK-47 profile, mirroring the original behaviour.
inline const WeaponProfile& getWeaponProfile(const std::string& type)
{
    static const WeaponProfile kAk47 = {
            .name = "AK-47",
            .type = "ak47",
            .obj_path = nx3d::projectAssetPath("ak47/source/ak47.obj"),
            .mtl_path = nx3d::projectAssetPath("ak47/source/ak47.mtl"),
            .ground_scale = 48.0f,
            .fp_scale = 1.35f,
            .fp_center = glm::vec3(0.0053f, 0.0156f, -0.0489f),
            .half_extents = btVector3(3.46f, 0.67f, 11.58f),
    };

    static const WeaponProfile kM4 = {
            .name = "M4",
            .type = "m4",
            .obj_path = nx3d::projectAssetPath("m4/source/m4final clean.obj"),
            .mtl_path = nx3d::projectAssetPath("m4/source/m4final clean.mtl"),
            .ground_scale = 1.45f,
            .fp_scale = 0.04f,
            .fp_center = glm::vec3(-0.095f, -0.183f, 2.40f),
            .half_extents = btVector3(3.07f, 1.13f, 12.07f),
    };

    if (type == "m4")
        return kM4;
    return kAk47;
}

} // namespace nx3d::client::gamemode

#endif