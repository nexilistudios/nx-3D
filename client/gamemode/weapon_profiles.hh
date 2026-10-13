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
    glm::vec3 fp_offset{0.68f, -0.58f, 2.25f};
    glm::vec3 fp_rotation{-10.0f, 33.0f, -12.0f};
    btVector3 half_extents{1.0f, 1.0f, 1.0f};
    /// Fully-automatic fire rate in rounds per minute (LMB held down).
    float rounds_per_minute = 600.0f;
    int magazine_size = 30;
    int reserve_magazines = 3;
    float reload_seconds = 2.5f;
    float damage = 35.0f;
    bool automatic = true;
};

/// Whether a weapon profile is known for the given item type.
inline bool hasWeaponProfile(const std::string& type)
{
    return type == "ak47" || type == "m16" || type == "glock" ||
           type == "usp" || type == "deagle" || type == "mp5";
}

/// Get the profile for a weapon type. Unknown types fall back
/// to the AK-47 profile, mirroring the original behaviour.
inline const WeaponProfile& getWeaponProfile(const std::string& type)
{
    static const WeaponProfile kAk47 = {
            .name = "AK-47",
            .type = "ak47",
            .obj_path = nx3d::projectAssetPath("weapons/ak47.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/ak47.mtl"),
            .ground_scale = 7.4f,
            .fp_scale = 0.50f,
            .fp_center = glm::vec3(0.0f, -0.05f, 0.20f),
            .fp_offset = glm::vec3(0.68f, -0.58f, 2.25f),
            .fp_rotation = glm::vec3(-10.0f, 33.0f, -12.0f),
            .half_extents = btVector3(1.0f, 4.4f, 11.7f),
            .rounds_per_minute = 600.0f,
            .magazine_size = 30,
            .reload_seconds = 2.5f,
    };

    static const WeaponProfile kM16 = {
            .name = "M16",
            .type = "m16",
            .obj_path = nx3d::projectAssetPath("weapons/m16.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/m16.mtl"),
            .ground_scale = 7.3f,
            .fp_scale = 0.50f,
            .fp_center = glm::vec3(0.0f, -0.05f, 0.30f),
            .fp_offset = glm::vec3(0.70f, -0.59f, 2.30f),
            .fp_rotation = glm::vec3(-9.0f, 35.0f, -11.0f),
            .half_extents = btVector3(1.0f, 4.2f, 11.7f),
            .rounds_per_minute = 800.0f,
            .magazine_size = 30,
            .reload_seconds = 2.5f,
    };

    static const WeaponProfile kGlock = {
            .name = "Glock", .type = "glock",
            .obj_path = nx3d::projectAssetPath("weapons/glock.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/glock.mtl"),
            .ground_scale = 8.0f, .fp_scale = 0.70f,
            .fp_center = glm::vec3(0.0f, -0.15f, 0.10f),
            .fp_offset = glm::vec3(0.72f, -0.66f, 2.05f),
            .fp_rotation = glm::vec3(-10.0f, 33.0f, -12.0f),
            .half_extents = btVector3(1.0f, 3.0f, 6.0f),
            .rounds_per_minute = 400.0f, .magazine_size = 20,
            .reload_seconds = 2.0f, .damage = 24.0f, .automatic = false,
    };
    static const WeaponProfile kUsp = {
            .name = "USP", .type = "usp",
            .obj_path = nx3d::projectAssetPath("weapons/usp.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/usp.mtl"),
            .ground_scale = 8.0f, .fp_scale = 0.70f,
            .fp_center = glm::vec3(0.0f, -0.15f, 0.10f),
            .fp_offset = glm::vec3(0.72f, -0.66f, 2.05f),
            .fp_rotation = glm::vec3(-10.0f, 33.0f, -12.0f),
            .half_extents = btVector3(1.0f, 3.0f, 6.0f),
            .rounds_per_minute = 350.0f, .magazine_size = 12,
            .reload_seconds = 2.1f, .damage = 30.0f, .automatic = false,
    };
    static const WeaponProfile kDeagle = {
            .name = "Desert Eagle", .type = "deagle",
            .obj_path = nx3d::projectAssetPath("weapons/deagle.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/deagle.mtl"),
            .ground_scale = 8.0f, .fp_scale = 0.70f,
            .fp_center = glm::vec3(0.0f, -0.15f, 0.10f),
            .fp_offset = glm::vec3(0.72f, -0.66f, 2.05f),
            .fp_rotation = glm::vec3(-10.0f, 33.0f, -12.0f),
            .half_extents = btVector3(1.0f, 3.0f, 6.0f),
            .rounds_per_minute = 260.0f, .magazine_size = 7,
            .reload_seconds = 2.2f, .damage = 35.0f, .automatic = false,
    };
    static const WeaponProfile kMp5 = {
            .name = "MP5", .type = "mp5",
            .obj_path = nx3d::projectAssetPath("weapons/mp5.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/mp5.mtl"),
            .ground_scale = 7.8f, .fp_scale = 0.56f,
            .fp_center = glm::vec3(0.0f, -0.08f, 0.20f),
            .fp_offset = glm::vec3(0.68f, -0.59f, 2.22f),
            .fp_rotation = glm::vec3(-10.0f, 33.0f, -12.0f),
            .half_extents = btVector3(1.0f, 3.8f, 10.0f),
            .rounds_per_minute = 800.0f, .magazine_size = 30,
            .reload_seconds = 2.4f, .damage = 26.0f, .automatic = true,
    };
    static const WeaponProfile kKnife = {
            .name = "Knife", .type = "knife",
            .obj_path = nx3d::projectAssetPath("weapons/knife.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/knife.mtl"),
            .fp_scale = 0.85f,
            .fp_offset = glm::vec3(0.7f, -0.7f, 1.8f),
            .rounds_per_minute = 100.0f, .magazine_size = 0,
            .damage = 35.0f, .automatic = false,
    };
    static const WeaponProfile kGrenade = {
            .name = "HE Grenade", .type = "grenade",
            .obj_path = nx3d::projectAssetPath("weapons/grenade.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/grenade.mtl"),
            .fp_scale = 0.75f,
            .fp_offset = glm::vec3(0.7f, -0.7f, 1.8f),
            .magazine_size = 0, .automatic = false,
    };
    static const WeaponProfile kSmoke = {
            .name = "Smoke Grenade", .type = "smoke",
            .obj_path = nx3d::projectAssetPath("weapons/grenade.obj"),
            .mtl_path = nx3d::projectAssetPath("weapons/grenade.mtl"),
            .fp_scale = 0.75f,
            .fp_offset = glm::vec3(0.7f, -0.7f, 1.8f),
            .magazine_size = 0, .automatic = false,
    };

    if (type == "knife") return kKnife;
    if (type == "grenade") return kGrenade;
    if (type == "smoke") return kSmoke;
    if (type == "glock") return kGlock;
    if (type == "usp") return kUsp;
    if (type == "deagle") return kDeagle;
    if (type == "mp5") return kMp5;
    if (type == "m16")
        return kM16;
    return kAk47;
}

} // namespace nx3d::client::gamemode

#endif
