#include <client/gun/first_person_gun.hh>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace nx3d::client::gun
{

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

FirstPersonGun::FirstPersonGun(VkDevice device,
                               VkPhysicalDevice physDevice,
                               VkCommandPool commandPool,
                               VkQueue graphicsQueue,
                               const std::string& object_path,
                               const std::string& material_path,
                               VkDescriptorPool descriptorPool,
                               VkDescriptorSetLayout descriptorSetLayout,
                               spear::physics::bullet::ObjectData&& object_data,
                               float model_scale,
                               glm::vec3 model_center)
    : OBJModel(device, physDevice, commandPool, graphicsQueue,
               object_path, material_path, descriptorPool, descriptorSetLayout,
               std::move(object_data), /* flip_winding = */ false),
      m_modelScale(model_scale),
      m_modelCenter(model_center)
{
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void FirstPersonGun::render(spear::Camera& camera)
{
    // Draw directly in camera space. Moving the mesh into world coordinates
    // and back loses precision at the map's large coordinates and makes a
    // nearby weapon visibly tremble as the player moves or turns.
    glm::vec3 offset = m_viewOffset + m_motion.bob;
    offset.y += m_motion.recoil;
    offset.z = -offset.z; // Positive forward distance becomes view-space -Z.
    glm::mat4 model = glm::translate(glm::mat4(1.0f), offset);

    // Apply view rotation offsets (tilt the gun)
    model = glm::rotate(model, glm::radians(m_viewRotation.x + m_motion.recoil * 10.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_viewRotation.y),
                        glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_viewRotation.z),
                        glm::vec3(0.0f, 0.0f, 1.0f));

    // The model was built facing +Z, rotate 180 around Y so it points toward -Z
    // (away from the camera, into the screen)
    model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Center and size the selected weapon mesh in the camera frame.
    model = glm::scale(model, glm::vec3(m_modelScale));
    model = glm::translate(model, -m_modelCenter);

    setModel(model);
    // Match the player's field of view without inheriting world translation
    // or aim punch. Camera movement is already implicit in this local frame.
    const float fov = glm::degrees(2.0f * std::atan(1.0f / camera.getProjectionMatrix()[1][1]));
    spear::Camera viewCamera(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                             -90.0f, 0.0f, 0.0f, 0.0f, fov);
    OBJModel::render(viewCamera);
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

void FirstPersonGun::updateAnimation(float delta_time, const glm::vec3& velocity, bool grounded)
{
    m_motion.update(delta_time, velocity, grounded);
}

void FirstPersonGun::addRecoil(float amount)
{
    m_motion.kick(amount);
}

} // namespace nx3d::client::gun
