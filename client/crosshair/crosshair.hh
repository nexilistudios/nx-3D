#ifndef NX3D_CLIENT_CROSSHAIR_CROSSHAIR_HH
#define NX3D_CLIENT_CROSSHAIR_CROSSHAIR_HH

#include <client/crosshair/crosshair_settings.hh>
#include <spear/rendering/vulkan/texture/stb_texture.hh>

#include <array>
#include <unordered_map>

namespace nx3d::client
{
// Each swapchain command buffer owns its vertices, keeping edits and animation
// separate from frames still using the GPU.
class Crosshair
{
public:
    Crosshair(VkDevice device, VkPhysicalDevice physDevice, VkCommandPool commandPool,
              VkQueue graphicsQueue, VkDescriptorPool descriptorPool,
              VkDescriptorSetLayout descriptorSetLayout);
    ~Crosshair();
    Crosshair(const Crosshair&) = delete;
    Crosshair& operator=(const Crosshair&) = delete;
    void setSettings(const CrosshairSettings& settings);
    void render(int width, int height, float centerX = 0, float centerY = 0,
                float spread = 0, float zoom = 1, int slot = 0,
                std::array<float, 4> clip = {-1, -1, 1, 1});

private:
    VkDevice m_device;
    VkPhysicalDevice m_physDevice;
    VkCommandPool m_commandPool;
    VkQueue m_graphicsQueue;
    struct FrameBuffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
    };
    std::unordered_map<VkCommandBuffer, FrameBuffer> m_frames;
    FrameBuffer& frameBuffer(VkCommandBuffer commandBuffer);
    VkDescriptorPool m_descriptorPool;
    std::array<VkDescriptorSet, 2> m_descriptorSets{};
    std::array<std::shared_ptr<spear::rendering::vulkan::STBTexture>, 2> m_textures;
    CrosshairSettings m_settings;
};
} // namespace nx3d::client
#endif
