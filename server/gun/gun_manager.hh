#ifndef NX3D_SERVER_GUN_GUN_MANAGER_HH
#define NX3D_SERVER_GUN_GUN_MANAGER_HH

#include <server/gun/gun.hh>

#include <cstdint>
#include <vector>

namespace nx3d::server::gun
{

class GunManager
{
public:
    GunManager();

    const std::vector<GunDef>& getDefs() const { return m_defs; }
    const GunDef* getDef(uint32_t index) const;

    GunInstance* getInstance(uint64_t id);
    const GunInstance* getInstance(uint64_t id) const;

    uint64_t spawnInstance(uint32_t def_index, float x, float y, float z, uint64_t owner_id = 0);
    void removeInstance(uint64_t id);
    void setOwner(uint64_t instance_id, uint64_t owner_id);

    bool tryFire(uint64_t instance_id, float now);
    void reload(uint64_t instance_id);
    void update(float dt);

private:
    uint64_t nextId();

    std::vector<GunDef> m_defs;
    std::vector<GunInstance> m_instances;
    uint64_t m_nextId = 1;
};

} // namespace nx3d::server::gun

#endif
