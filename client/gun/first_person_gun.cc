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
    // Compute the world-space transform for the gun.
    // Place the gun at a fixed offset relative to the camera, then let the
    // inherited OBJModel render handle the view-projection transform. This
    // keeps the gun at a constant screen position regardless of camera motion.

    glm::vec3 offset = m_viewOffset + m_bobOffset;
    offset.y += m_recoilKick;

    glm::vec3 gunPos = camera.getPosition()
        + camera.getRight() * offset.x
        + camera.getUp() * offset.y
        + camera.getFront() * offset.z;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), gunPos);

    // Apply view rotation offsets (tilt the gun)
    model = glm::rotate(model, glm::radians(m_viewRotation.x + m_recoilKick * 10.0f),
                        camera.getRight());
    model = glm::rotate(model, glm::radians(m_viewRotation.y),
                        camera.getUp());
    model = glm::rotate(model, glm::radians(m_viewRotation.z),
                        camera.getFront());

    // The model was built facing +Z, rotate 180 around Y so it points toward -Z
    // (away from the camera, into the screen)
    model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Fit the AK model into the same visible space as the old procedural gun:
    // scale it up and center it around the view position.
    model = glm::scale(model, glm::vec3(m_modelScale));
    model = glm::translate(model, -m_modelCenter);

    setModel(model);
    OBJModel::render(camera);

    // Decay recoil each frame
    m_recoilKick *= 0.85f;
    if (m_recoilKick < 0.001f)
        m_recoilKick = 0.0f;
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

void FirstPersonGun::addBob(float delta_time, const glm::vec3& velocity)
{
    float speed = glm::length(velocity);
    if (speed > 0.1f)
    {
        m_bobPhase += delta_time * speed * 0.5f;
        m_bobOffset.x = std::sin(m_bobPhase) * 0.008f;
        m_bobOffset.y = std::abs(std::cos(m_bobPhase)) * 0.008f;
    }
    else
    {
        m_bobPhase = 0.0f;
        m_bobOffset = glm::vec3(0.0f);
    }
}

void FirstPersonGun::addRecoil(float amount)
{
    m_recoilKick += amount;
    if (m_recoilKick > 0.15f)
        m_recoilKick = 0.15f;
}

} // namespace nx3d::client::gun