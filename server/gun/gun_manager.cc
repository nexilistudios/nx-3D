#include <server/gun/gun_manager.hh>

#include <algorithm>

namespace nx3d::server::gun
{

GunManager::GunManager()
{
    m_defs = {
        {
            .name = "Pistol",
            .type = GunType::Pistol,
            .damage = 25.0f,
            .fire_rate = 4.0f,
            .magazine_size = 12,
            .reload_time = 1.5f,
            .range = 50.0f,
            .spread = 2.0f,
            .model_path = "assets/guns/pistol.obj",
        },
        {
            .name = "Rifle",
            .type = GunType::Rifle,
            .damage = 35.0f,
            .fire_rate = 10.0f,
            .magazine_size = 30,
            .reload_time = 2.5f,
            .range = 100.0f,
            .spread = 1.0f,
            .model_path = "assets/guns/rifle.obj",
        },
        {
            .name = "SMG",
            .type = GunType::SMG,
            .damage = 20.0f,
            .fire_rate = 12.0f,
            .magazine_size = 35,
            .reload_time = 2.0f,
            .range = 40.0f,
            .spread = 3.0f,
            .model_path = "assets/guns/smg.obj",
        },
        {
            .name = "Shotgun",
            .type = GunType::Shotgun,
            .damage = 15.0f, // per pellet
            .fire_rate = 1.0f,
            .magazine_size = 8,
            .reload_time = 3.0f,
            .range = 20.0f,
            .spread = 8.0f,
            .model_path = "assets/guns/shotgun.obj",
        },
        {
            .name = "Sniper",
            .type = GunType::Sniper,
            .damage = 100.0f,
            .fire_rate = 1.5f,
            .magazine_size = 5,
            .reload_time = 3.5f,
            .range = 200.0f,
            .spread = 0.2f,
            .model_path = "assets/guns/sniper.obj",
        },
    };
}

const GunDef* GunManager::getDef(uint32_t index) const
{
    if (index < m_defs.size())
        return &m_defs[index];
    return nullptr;
}

GunInstance* GunManager::getInstance(uint64_t id)
{
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                           [id](const auto& inst) { return inst.id == id && inst.active; });
    return it != m_instances.end() ? &(*it) : nullptr;
}

const GunInstance* GunManager::getInstance(uint64_t id) const
{
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                           [id](const auto& inst) { return inst.id == id && inst.active; });
    return it != m_instances.end() ? &(*it) : nullptr;
}

uint64_t GunManager::spawnInstance(uint32_t def_index, float x, float y, float z, uint64_t owner_id)
{
    if (def_index >= m_defs.size())
        return 0;

    auto& def = m_defs[def_index];
    GunInstance inst;
    inst.id = nextId();
    inst.def_index = def_index;
    inst.current_ammo = def.magazine_size;
    inst.last_fire_time = 0.0f;
    inst.owner_id = owner_id;
    inst.pos_x = x;
    inst.pos_y = y;
    inst.pos_z = z;
    inst.active = true;
    m_instances.push_back(std::move(inst));
    return inst.id;
}

void GunManager::removeInstance(uint64_t id)
{
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                           [id](const auto& inst) { return inst.id == id; });
    if (it != m_instances.end())
        it->active = false;
}

void GunManager::setOwner(uint64_t instance_id, uint64_t owner_id)
{
    auto* inst = getInstance(instance_id);
    if (inst)
        inst->owner_id = owner_id;
}

bool GunManager::tryFire(uint64_t instance_id, float now)
{
    auto* inst = getInstance(instance_id);
    if (!inst)
        return false;
    auto* def = getDef(inst->def_index);
    if (!def)
        return false;
    if (inst->canFire(now, def->fire_rate))
    {
        inst->fire(now);
        return true;
    }
    return false;
}

void GunManager::reload(uint64_t instance_id)
{
    auto* inst = getInstance(instance_id);
    if (!inst)
        return;
    auto* def = getDef(inst->def_index);
    if (!def)
        return;
    inst->reload(def->magazine_size);
}

void GunManager::update(float dt)
{
    (void)dt;
}

uint64_t GunManager::nextId()
{
    return m_nextId++;
}

} // namespace nx3d::server::gun
