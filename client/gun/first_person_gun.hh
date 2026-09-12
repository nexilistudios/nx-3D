#ifndef NX3D_CLIENT_GUN_FIRST_PERSON_GUN_HH
#define NX3D_CLIENT_GUN_FIRST_PERSON_GUN_HH

#include <spear/rendering/vulkan/model/obj_model.hh>

#include <glm/glm.hpp>

namespace nx3d::client::gun
{

/// First-person gun renderable.
/// Reuses the exact same OBJ model as the ground pickups, rendered at a
/// fixed view-space position (always in front of the camera, like CS:GO).
class FirstPersonGun : public spear::rendering::vulkan::OBJModel
{
public:
    /// Construct with the weapon OBJ/MTL paths (same assets as the map guns).
    /// model_scale / model_center are per-weapon view parameters (bounds-based
    /// centering + scale), defaulting to the AK-47 profile.
    FirstPersonGun(VkDevice device,
                   VkPhysicalDevice physDevice,
                   VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::string& object_path,
                   const std::string& material_path,
                   VkDescriptorPool descriptorPool,
                   VkDescriptorSetLayout descriptorSetLayout,
                   spear::physics::bullet::ObjectData&& object_data,
                   float model_scale = 1.35f,
                   glm::vec3 model_center = glm::vec3(0.0053f, 0.0156f, -0.0489f));

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
    float m_modelScale = 1.35f;
    glm::vec3 m_modelCenter{0.0053f, 0.0156f, -0.0489f};

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