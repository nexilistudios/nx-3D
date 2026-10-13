#include <server/gun/gun_manager.hh>

#include <algorithm>

namespace nx3d::server::gun
{

GunManager::GunManager()
{
    m_defs = {
        {
            .name = "AK-47",
            .type = GunType::AK47,
            .damage = 35.0f,
            .fire_rate = 10.0f,
            .magazine_size = 30,
            .reload_time = 2.5f,
            .range = 100.0f,
            .spread = 1.0f,
            .model_path = "assets/weapons/ak47.obj",
        },
        {
            .name = "M16",
            .type = GunType::M16,
            .damage = 35.0f,
            .fire_rate = 13.33f,
            .magazine_size = 30,
            .reload_time = 2.5f,
            .range = 100.0f,
            .spread = 1.0f,
            .model_path = "assets/weapons/m16.obj",
        },
        {
            .name = "Glock", .type = GunType::Glock, .damage = 24.0f,
            .fire_rate = 6.67f, .magazine_size = 20, .reload_time = 2.0f,
            .range = 100.0f, .spread = 1.0f,
            .model_path = "assets/weapons/glock.obj",
        },
        {
            .name = "USP", .type = GunType::USP, .damage = 30.0f,
            .fire_rate = 5.83f, .magazine_size = 12, .reload_time = 2.1f,
            .range = 100.0f, .spread = 1.0f,
            .model_path = "assets/weapons/usp.obj",
        },
        {
            .name = "Desert Eagle", .type = GunType::Deagle, .damage = 35.0f,
            .fire_rate = 4.33f, .magazine_size = 7, .reload_time = 2.2f,
            .range = 100.0f, .spread = 1.0f,
            .model_path = "assets/weapons/deagle.obj",
        },
        {
            .name = "MP5", .type = GunType::MP5, .damage = 26.0f,
            .fire_rate = 13.33f, .magazine_size = 30, .reload_time = 2.4f,
            .range = 100.0f, .spread = 1.0f,
            .model_path = "assets/weapons/mp5.obj",
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
