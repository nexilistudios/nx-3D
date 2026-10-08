#ifndef NX3D_CLIENT_GUN_FIRST_PERSON_GUN_HH
#define NX3D_CLIENT_GUN_FIRST_PERSON_GUN_HH

#include <client/gun/weapon_motion.hh>

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
                   float model_scale = 3.0f,
                   glm::vec3 model_center = glm::vec3(0.0053f, 0.0156f, -0.0489f));

    void render(spear::Camera& camera) override;

    // --- Animation -----------------------------------------------------------

    // Offset relative to camera (right, up, forward) in world units.
    void setViewOffset(glm::vec3 offset)
    {
        m_viewOffset = offset;
    }
    void setViewRotation(glm::vec3 rotation)
    {
        m_viewRotation = rotation;
    }

    // Call every frame with player velocity to produce walk-bob.
    void updateAnimation(float delta_time, const glm::vec3& velocity, bool grounded);
    void resetAnimation()
    {
        m_motion = {};
    }

    // Call on fire to produce a quick kick.
    void addRecoil(float amount);

private:
    float m_modelScale = 3.0f;
    glm::vec3 m_modelCenter{0.0053f, 0.0156f, -0.0489f};

    // View-space offset/rotation
    glm::vec3 m_viewOffset{0.55f, -0.45f, 2.5f};
    glm::vec3 m_viewRotation{-8.0f, 25.0f, -8.0f};

    // Animation state
    WeaponMotion m_motion;
};

} // namespace nx3d::client::gun

#endif
