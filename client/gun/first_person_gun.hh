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
                   float model_scale = 0.50f,
                   glm::vec3 model_center = glm::vec3(0.0f, -0.05f, 0.20f),
                   bool separated_magazine = false);

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
    void startReload(float duration);

private:
    float m_modelScale = 0.50f;
    glm::vec3 m_modelCenter{0.0f, -0.05f, 0.20f};

    // View-space offset/rotation
    glm::vec3 m_viewOffset{0.68f, -0.58f, 2.25f};
    glm::vec3 m_viewRotation{-10.0f, 33.0f, -12.0f};

    // Animation state
    WeaponMotion m_motion;
    std::unique_ptr<spear::rendering::vulkan::OBJModel> m_magazine;
};

} // namespace nx3d::client::gun

#endif
