#include <client/gun/first_person_gun.hh>

#include <spear/rendering/vulkan/frame_context.hh>

#include <glm/gtc/matrix_transform.hpp>

#include <cstring>
#include <stdexcept>

namespace nx3d::client::gun
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void pushQuad(std::vector<FirstPersonGun::Vertex>& verts,
                     glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                     glm::vec2 uv_a, glm::vec2 uv_b,
                     glm::vec2 uv_c, glm::vec2 uv_d)
{
    verts.push_back({a, uv_a});
    verts.push_back({b, uv_b});
    verts.push_back({c, uv_c});
    verts.push_back({a, uv_a});
    verts.push_back({c, uv_c});
    verts.push_back({d, uv_d});
}

static void pushBox(std::vector<FirstPersonGun::Vertex>& verts,
                    float x0, float y0, float z0,
                    float x1, float y1, float z1)
{
    pushQuad(verts, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
             {0,1},{1,1},{1,0},{0,0});
    pushQuad(verts, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
             {0,1},{1,1},{1,0},{0,0});
    pushQuad(verts, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0},
             {0,1},{1,1},{1,0},{0,0});
    pushQuad(verts, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
             {0,1},{1,1},{1,0},{0,0});
    pushQuad(verts, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0},
             {0,1},{1,1},{1,0},{0,0});
    pushQuad(verts, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
             {0,1},{1,1},{1,0},{0,0});
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

FirstPersonGun::FirstPersonGun(VkDevice device,
                               VkPhysicalDevice physDevice,
                               std::shared_ptr<spear::rendering::vulkan::Texture> texture,
                               VkDescriptorPool descriptorPool,
                               VkDescriptorSetLayout descriptorSetLayout,
                               spear::physics::bullet::ObjectData&& object_data)
    : Shape(nullptr, std::move(object_data), glm::vec4(1.0f)),
      m_device(device),
      m_physDevice(physDevice),
      m_texture(std::move(texture))
{
    buildGunMesh();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("FirstPersonGun: failed to allocate descriptor set");

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_texture->getImageView();
    imageInfo.sampler = m_texture->getSampler();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

FirstPersonGun::~FirstPersonGun()
{
    if (m_vertexBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    if (m_vertexMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_vertexMemory, nullptr);
}

// ---------------------------------------------------------------------------
// Mesh construction
// ---------------------------------------------------------------------------

std::vector<FirstPersonGun::Vertex> FirstPersonGun::createVertices() const
{
    std::vector<Vertex> verts;

    // Barrel (long thin box at the top front)
    pushBox(verts, -0.04f, -0.02f, 0.15f, 0.04f, 0.06f, 0.50f);

    // Slide / upper receiver
    pushBox(verts, -0.07f, -0.02f, -0.05f, 0.07f, 0.10f, 0.20f);

    // Lower receiver / body
    pushBox(verts, -0.09f, -0.12f, -0.08f, 0.09f, 0.00f, 0.15f);

    // Trigger guard
    pushBox(verts, -0.06f, -0.20f, 0.00f, 0.06f, -0.12f, 0.10f);

    // Grip
    pushBox(verts, -0.06f, -0.35f, -0.04f, 0.06f, -0.12f, 0.06f);

    // Magazine well
    pushBox(verts, -0.06f, -0.28f, -0.06f, 0.06f, -0.12f, 0.00f);

    // Sight
    pushBox(verts, -0.03f, 0.10f, 0.10f, 0.03f, 0.14f, 0.13f);

    return verts;
}

void FirstPersonGun::buildGunMesh()
{
    auto vertices = createVertices();
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS)
        throw std::runtime_error("FirstPersonGun: failed to create vertex buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_physDevice, memReq.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_vertexMemory) != VK_SUCCESS)
        throw std::runtime_error("FirstPersonGun: failed to allocate vertex memory");

    vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexMemory, 0);

    void* data;
    vkMapMemory(m_device, m_vertexMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_device, m_vertexMemory);
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void FirstPersonGun::render(spear::Camera& camera)
{
    VkCommandBuffer cmd = spear::rendering::vulkan::g_frameContext.commandBuffer;
    VkPipeline pipeline = spear::rendering::vulkan::g_frameContext.texturedPipeline;
    VkPipelineLayout layout = spear::rendering::vulkan::g_frameContext.texturedPipelineLayout;
    if (cmd == VK_NULL_HANDLE || pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                            0, 1, &m_descriptorSet, 0, nullptr);

    // Compute view-space transform for the gun.
    // Place the gun at a fixed offset relative to the camera in world space,
    // then transform by view-projection. This keeps the gun at a constant
    // screen position regardless of camera movement/rotation.

    glm::vec3 offset = m_viewOffset + m_bobOffset;
    offset.y += m_recoilKick;

    // World-space gun position from camera-relative offset
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

    // The mesh was built facing +Z, rotate 180 around Y so it points toward -Z
    // (away from the camera, into the screen)
    model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 mvp = camera.getProjectionMatrix() * camera.getViewMatrix() * model;

    VkBuffer buffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdPushConstants(cmd, layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(glm::mat4), &mvp);
    vkCmdDraw(cmd, m_vertexCount, 1, 0, 0);

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

uint32_t FirstPersonGun::findMemoryType(VkPhysicalDevice physDevice,
                                        uint32_t typeFilter,
                                        VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("FirstPersonGun: failed to find suitable memory type!");
}

} // namespace nx3d::client::gun
