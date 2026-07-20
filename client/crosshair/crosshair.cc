#include <client/crosshair/crosshair.hh>

#include <spear/rendering/vulkan/frame_context.hh>

#include <cstring>
#include <stdexcept>

namespace nx3d::client
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void pushQuad(std::vector<Crosshair::Vertex>& verts,
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

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

Crosshair::Crosshair(VkDevice device,
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
    buildMesh();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Crosshair: failed to allocate descriptor set");

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

Crosshair::~Crosshair()
{
    if (m_vertexBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    if (m_vertexMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_vertexMemory, nullptr);
}

// ---------------------------------------------------------------------------
// Mesh construction
// ---------------------------------------------------------------------------

std::vector<Crosshair::Vertex> Crosshair::createVertices() const
{
    std::vector<Vertex> verts;

    // CS-style crosshair: 4 arms with a small center gap.
    // Coordinates in NDC (-1 to 1). Center is (0, 0).
    float gap = 0.008f;   // Half-gap from center
    float armLen = 0.025f; // Length of each arm
    float thickness = 0.002f; // Half-thickness of each arm

    // Top arm: from (0, gap) to (0, gap+armLen)
    pushQuad(verts,
             {-thickness, gap, 0.0f},
             { thickness, gap, 0.0f},
             { thickness, gap + armLen, 0.0f},
             {-thickness, gap + armLen, 0.0f},
             {0, 0}, {1, 0}, {1, 1}, {0, 1});

    // Bottom arm: from (0, -gap) to (0, -gap-armLen)
    pushQuad(verts,
             {-thickness, -gap - armLen, 0.0f},
             { thickness, -gap - armLen, 0.0f},
             { thickness, -gap, 0.0f},
             {-thickness, -gap, 0.0f},
             {0, 0}, {1, 0}, {1, 1}, {0, 1});

    // Left arm: from (-gap, 0) to (-gap-armLen, 0)
    pushQuad(verts,
             {-gap - armLen, -thickness, 0.0f},
             {-gap,         -thickness, 0.0f},
             {-gap,          thickness, 0.0f},
             {-gap - armLen,  thickness, 0.0f},
             {0, 0}, {1, 0}, {1, 1}, {0, 1});

    // Right arm: from (gap, 0) to (gap+armLen, 0)
    pushQuad(verts,
             { gap,         -thickness, 0.0f},
             { gap + armLen,-thickness, 0.0f},
             { gap + armLen, thickness, 0.0f},
             { gap,          thickness, 0.0f},
             {0, 0}, {1, 0}, {1, 1}, {0, 1});

    return verts;
}

void Crosshair::buildMesh()
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
        throw std::runtime_error("Crosshair: failed to create vertex buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_physDevice, memReq.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_vertexMemory) != VK_SUCCESS)
        throw std::runtime_error("Crosshair: failed to allocate vertex memory");

    vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexMemory, 0);

    void* data;
    vkMapMemory(m_device, m_vertexMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_device, m_vertexMemory);
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void Crosshair::render(spear::Camera& camera)
{
    VkCommandBuffer cmd = spear::rendering::vulkan::g_frameContext.commandBuffer;
    VkPipeline pipeline = spear::rendering::vulkan::g_frameContext.uiPipeline;
    VkPipelineLayout layout = spear::rendering::vulkan::g_frameContext.uiPipelineLayout;
    if (cmd == VK_NULL_HANDLE || pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                            0, 1, &m_descriptorSet, 0, nullptr);

    VkBuffer buffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdDraw(cmd, m_vertexCount, 1, 0, 0);
}

// ---------------------------------------------------------------------------
// Memory helper
// ---------------------------------------------------------------------------

uint32_t Crosshair::findMemoryType(VkPhysicalDevice physDevice,
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
    throw std::runtime_error("Crosshair: failed to find suitable memory type!");
}

} // namespace nx3d::client
