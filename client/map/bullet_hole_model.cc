#include <client/map/bullet_hole_model.hh>

#include <spear/rendering/vulkan/frame_context.hh>

#include <glm/geometric.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace nx3d::client
{
namespace
{
struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
};

std::vector<Vertex> makeVertices(const nx3d::BulletHole& hole)
{
    constexpr int segments = 12;
    constexpr float pi = 3.14159265358979323846f;
    const glm::vec3 normal = glm::normalize(hole.normal);
    const glm::vec3 reference = std::abs(normal.y) < 0.9f ? glm::vec3(0, 1, 0)
                                                          : glm::vec3(1, 0, 0);
    const glm::vec3 right = glm::normalize(glm::cross(reference, normal));
    const glm::vec3 up = glm::cross(normal, right);
    const glm::vec3 center = hole.position + normal * 0.12f;
    const glm::vec3 soot(0.015f, 0.015f, 0.014f);
    const glm::vec3 rim(0.22f, 0.21f, 0.19f);
    auto point = [&](float radius, int index)
    {
        const float angle = 2.0f * pi * static_cast<float>(index) / segments;
        return center + radius * (std::cos(angle) * right + std::sin(angle) * up);
    };
    std::vector<Vertex> vertices;
    vertices.reserve(segments * 9);
    for (int i = 0; i < segments; ++i)
    {
        const auto innerA = point(1.5f, i);
        const auto innerB = point(1.5f, i + 1);
        const auto outerA = point(2.5f, i);
        const auto outerB = point(2.5f, i + 1);
        vertices.insert(vertices.end(), {
                {center, soot}, {innerA, soot}, {innerB, soot},
                {innerA, soot}, {outerA, rim}, {outerB, rim},
                {innerA, soot}, {outerB, rim}, {innerB, soot}});
    }
    return vertices;
}

uint32_t memoryType(VkPhysicalDevice physicalDevice, uint32_t allowed)
{
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((allowed & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            return i;
    throw std::runtime_error("Cannot allocate bullet hole vertex memory");
}
} // namespace

BulletHoleModel::BulletHoleModel(VkDevice device, VkPhysicalDevice physicalDevice,
                                 const nx3d::BulletHole& hole)
    : GameObject(spear::physics::bullet::ObjectData({}, 0.0f, glm::vec3(0), glm::vec3(1)), nullptr),
      m_device(device)
{
    const auto vertices = makeVertices(hole);
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    const VkDeviceSize bytes = vertices.size() * sizeof(Vertex);
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = bytes;
    buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer, nullptr, &m_buffer) != VK_SUCCESS)
        throw std::runtime_error("Cannot create bullet hole vertex buffer");
    try
    {
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, m_buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType(physicalDevice, requirements.memoryTypeBits);
        if (vkAllocateMemory(device, &allocation, nullptr, &m_memory) != VK_SUCCESS ||
            vkBindBufferMemory(device, m_buffer, m_memory, 0) != VK_SUCCESS)
            throw std::runtime_error("Cannot allocate bullet hole vertex buffer");
        void* mapped = nullptr;
        if (vkMapMemory(device, m_memory, 0, bytes, 0, &mapped) != VK_SUCCESS)
            throw std::runtime_error("Cannot upload bullet hole vertices");
        std::memcpy(mapped, vertices.data(), bytes);
        vkUnmapMemory(device, m_memory);
    }
    catch (...)
    {
        if (m_memory) vkFreeMemory(device, m_memory, nullptr);
        vkDestroyBuffer(device, m_buffer, nullptr);
        throw;
    }
}

BulletHoleModel::~BulletHoleModel()
{
    if (m_buffer) vkDestroyBuffer(m_device, m_buffer, nullptr);
    if (m_memory) vkFreeMemory(m_device, m_memory, nullptr);
}

void BulletHoleModel::render(spear::Camera& camera)
{
    const auto& frame = spear::rendering::vulkan::g_frameContext;
    if (frame.shadowPass || !frame.commandBuffer || !frame.colorPipeline || !frame.colorPipelineLayout)
        return;
    const glm::mat4 mvp = camera.getProjectionMatrix() * camera.getViewMatrix();
    const glm::vec4 emission(0.0f);
    const VkDeviceSize offset = 0;
    vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frame.colorPipeline);
    vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            frame.colorPipelineLayout, 0, 1, &frame.sunDescriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &m_buffer, &offset);
    vkCmdPushConstants(frame.commandBuffer, frame.colorPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(mvp), &mvp);
    vkCmdPushConstants(frame.commandBuffer, frame.colorPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(mvp), sizeof(emission), &emission);
    vkCmdDraw(frame.commandBuffer, m_vertexCount, 1, 0, 0);
}

} // namespace nx3d::client
