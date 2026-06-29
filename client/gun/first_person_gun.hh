#ifndef NX3D_CLIENT_GUN_FIRST_PERSON_GUN_HH
#define NX3D_CLIENT_GUN_FIRST_PERSON_GUN_HH

#include <spear/rendering/vulkan/texture/texture.hh>
#include <spear/rendering/shapes/shape.hh>

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace nx3d::client::gun
{

/// First-person gun renderable.
/// Renders a textured gun model at a fixed view-space position
/// (always in front of the camera, like CS:GO).
class FirstPersonGun : public spear::rendering::Shape
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec2 uv;
    };

    /// Construct with a texture for the gun skin.
    FirstPersonGun(VkDevice device,
                   VkPhysicalDevice physDevice,
                   std::shared_ptr<spear::rendering::vulkan::Texture> texture,
                   VkDescriptorPool descriptorPool,
                   VkDescriptorSetLayout descriptorSetLayout,
                   spear::physics::bullet::ObjectData&& object_data);

    ~FirstPersonGun();

    void render(spear::Camera& camera) override;

    // --- Animation -----------------------------------------------------------

    // Offset relative to camera (right, up, forward) in view space (~screen coords).
    void setViewOffset(glm::vec3 offset) { m_viewOffset = offset; }
    void setViewRotation(glm::vec3 rotation) { m_viewRotation = rotation; }

    // Call every frame with player velocity to produce walk-bob.
    void addBob(float delta_time, const glm::vec3& velocity);

    // Call on fire to produce a quick kick.
    void addRecoil(float amount);

private:
    void buildGunMesh();
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

    // View-space offset/rotation
    glm::vec3 m_viewOffset{0.25f, -0.25f, -0.4f};
    glm::vec3 m_viewRotation{-5.0f, 0.0f, 0.0f};

    // Animation state
    float m_bobPhase = 0.0f;
    glm::vec3 m_bobOffset{0.0f};
    float m_recoilKick = 0.0f;
};

} // namespace nx3d::client::gun

#endif
