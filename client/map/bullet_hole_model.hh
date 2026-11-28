#ifndef NX3D_CLIENT_MAP_BULLET_HOLE_MODEL_HH
#define NX3D_CLIENT_MAP_BULLET_HOLE_MODEL_HH

#include <shared/bullet_hole.hh>
#include <spear/game_object.hh>

#include <vulkan/vulkan.h>

namespace nx3d::client
{

class BulletHoleModel : public spear::GameObject
{
public:
    BulletHoleModel(VkDevice device, VkPhysicalDevice physicalDevice,
                    const nx3d::BulletHole& hole);
    ~BulletHoleModel();
    void render(spear::Camera& camera) override;

private:
    VkDevice m_device;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    uint32_t m_vertexCount = 0;
};

} // namespace nx3d::client

#endif
