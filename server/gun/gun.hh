#ifndef NX3D_SERVER_GUN_GUN_HH
#define NX3D_SERVER_GUN_GUN_HH

#include <array>
#include <cstdint>
#include <string>

namespace nx3d::server::gun
{

enum class GunType : uint8_t
{
    Pistol,
    Rifle,
    SMG,
    Shotgun,
    Sniper
};

struct GunDef
{
    std::string name;
    GunType type;
    float damage;          // per hit
    float fire_rate;       // rounds per second
    uint32_t magazine_size; // max rounds per mag
    float reload_time;     // seconds
    float range;           // max effective range
    float spread;          // base spread in degrees
    std::string model_path; // OBJ path for client rendering
};

struct GunInstance
{
    uint64_t id;             // unique instance id
    uint32_t def_index;      // index into GunManager's defs
    uint32_t current_ammo;   // rounds left in mag
    float last_fire_time;    // server tick time of last shot
    uint64_t owner_id;       // 0 if on ground
    float pos_x, pos_y, pos_z; // world position (when on ground)
    bool active;

    bool canFire(float now, float fire_rate) const
    {
        return current_ammo > 0 && (now - last_fire_time) >= (1.0f / fire_rate);
    }

    void fire(float now)
    {
        if (current_ammo > 0)
        {
            --current_ammo;
            last_fire_time = now;
        }
    }

    void reload(uint32_t magazine_size)
    {
        current_ammo = magazine_size;
    }
};

} // namespace nx3d::server::gun

#endif
