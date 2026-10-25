#ifndef NX3D_CLIENT_MAP_BRUSH_MODEL_HH
#define NX3D_CLIENT_MAP_BRUSH_MODEL_HH

#include <shared/brush_mesh.hh>
#include <spear/game_object.hh>
#include <spear/rendering/vulkan/texture/stb_texture.hh>

#include <memory>
#include <vector>

namespace nx3d::client
{
// One draw per material, regardless of the number of editable cubes.
class BrushModel : public spear::GameObject
{
public:
    BrushModel(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commands,
               VkQueue queue, VkDescriptorPool pool, VkDescriptorSetLayout layout,
               const BrushMesh& mesh);
    ~BrushModel();
    void render(spear::Camera& camera) override;

private:
    struct Batch
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
        uint32_t count = 0;
        std::shared_ptr<spear::rendering::vulkan::STBTexture> texture;
    };
    void release();
    VkDevice m_device;
    VkDescriptorPool m_pool;
    std::vector<Batch> m_batches;
};
}

#endif
