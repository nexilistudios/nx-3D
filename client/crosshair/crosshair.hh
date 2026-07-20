#ifndef NX3D_CLIENT_CROSSHAIR_CROSSHAIR_HH
#define NX3D_CLIENT_CROSSHAIR_CROSSHAIR_HH

#include <spear/rendering/shapes/shape.hh>
#include <spear/rendering/vulkan/texture/texture.hh>

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace nx3d::client
{

/// CS-style crosshair rendered as 4 thin white quads in a cross pattern.
/// Uses the UI pipeline to draw as a 2D overlay.
class Crosshair : public spear::rendering::Shape
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec2 uv;
    };

    Crosshair(VkDevice device,
              VkPhysicalDevice physDevice,
              std::shared_ptr<spear::rendering::vulkan::Texture> texture,
              VkDescriptorPool descriptorPool,
              VkDescriptorSetLayout descriptorSetLayout,
              spear::physics::bullet::ObjectData&& object_data);

    ~Crosshair();

    void render(spear::Camera& camera) override;

private:
    void buildMesh();
    std::vector<Vertex> createVertices() const;

    uint32_t findMemoryType(VkPhysicalDevice physDevice,
                            uint32_t typeFilter,
                            VkMemoryPropertyFlags properties);

    VkDevice m_device;
    VkPhysicalDevice m_physDevice;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;
    uint32_t m_vertexCount = 0;

    std::shared_ptr<spear::rendering::vulkan::Texture> m_texture;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
};

} // namespace nx3d::client

#endif
