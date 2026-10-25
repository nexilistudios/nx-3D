#include <client/map/brush_model.hh>
#include <client/project_root.hh>
#include <spear/rendering/vulkan/frame_context.hh>

#include <cstring>
#include <stdexcept>

namespace nx3d::client
{
namespace vk = spear::rendering::vulkan;

BrushModel::BrushModel(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commands,
                       VkQueue queue, VkDescriptorPool pool, VkDescriptorSetLayout layout,
                       const BrushMesh& mesh)
    : GameObject(spear::physics::bullet::ObjectData({}, 0.0f, glm::vec3(0), glm::vec3(1)), nullptr),
      m_device(device), m_pool(pool)
{
    try
    {
        for (std::size_t materialIndex = 0; materialIndex < kMapMaterials.size(); ++materialIndex)
        {
            const char* material = kMapMaterials[materialIndex];
            const auto& vertices = mesh[materialIndex];
            if (vertices.empty()) continue;
            m_batches.emplace_back();
            auto& batch = m_batches.back();
            batch.count = static_cast<uint32_t>(vertices.size());
            batch.texture = std::make_shared<vk::STBTexture>(device, physicalDevice, commands, queue);
            batch.texture->loadFromFile(projectAssetPath(std::string("maps/dust2_cs2/textures/") + material + ".png"));
            batch.descriptor = vk::Texture::allocateDescriptorSet(device, pool, layout,
                    batch.texture->getImageView(), batch.texture->getSampler());

            const VkDeviceSize size = vertices.size() * sizeof(MapVertex);
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            buffer.size = size;
            buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            if (vkCreateBuffer(device, &buffer, nullptr, &batch.buffer) != VK_SUCCESS)
                throw std::runtime_error("Cannot create generated map vertex buffer");
            VkMemoryRequirements requirements;
            vkGetBufferMemoryRequirements(device, batch.buffer, &requirements);
            VkPhysicalDeviceMemoryProperties properties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
            uint32_t type = properties.memoryTypeCount;
            for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
                if ((requirements.memoryTypeBits & (1u << i)) &&
                    (properties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                { type = i; break; }
            if (type == properties.memoryTypeCount)
                throw std::runtime_error("Cannot allocate generated map memory");
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = type;
            if (vkAllocateMemory(device, &allocation, nullptr, &batch.memory) != VK_SUCCESS ||
                vkBindBufferMemory(device, batch.buffer, batch.memory, 0) != VK_SUCCESS)
                throw std::runtime_error("Cannot bind generated map memory");
            void* mapped = nullptr;
            if (vkMapMemory(device, batch.memory, 0, size, 0, &mapped) != VK_SUCCESS)
                throw std::runtime_error("Cannot upload generated map");
            std::memcpy(mapped, vertices.data(), size);
            vkUnmapMemory(device, batch.memory);
        }
    }
    catch (...) { release(); throw; }
}

BrushModel::~BrushModel() { release(); }

void BrushModel::release()
{
    for (auto& batch : m_batches)
    {
        if (batch.descriptor) vkFreeDescriptorSets(m_device, m_pool, 1, &batch.descriptor);
        if (batch.buffer) vkDestroyBuffer(m_device, batch.buffer, nullptr);
        if (batch.memory) vkFreeMemory(m_device, batch.memory, nullptr);
    }
    m_batches.clear();
}

void BrushModel::render(spear::Camera& camera)
{
    const auto& frame = vk::g_frameContext;
    const auto pipeline = frame.shadowPass ? frame.shadowTexturedPipeline : frame.texturedPipeline;
    const auto layout = frame.shadowPass ? frame.shadowPipelineLayout : frame.texturedPipelineLayout;
    if (!frame.commandBuffer || !pipeline || !layout) return;
    const glm::mat4 mvp = frame.shadowPass ? frame.lightViewProjection :
                        camera.getProjectionMatrix() * camera.getViewMatrix();
    vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdPushConstants(frame.commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT |
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(mvp), &mvp);
    if (!frame.shadowPass)
    {
        const glm::vec4 params(0.0f);
        vkCmdPushConstants(frame.commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(mvp), sizeof(params), &params);
        vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                                1, 1, &frame.sunDescriptorSet, 0, nullptr);
    }
    for (const auto& batch : m_batches)
    {
        const VkDeviceSize offset = 0;
        if (!frame.shadowPass)
            vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                                    0, 1, &batch.descriptor, 0, nullptr);
        vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &batch.buffer, &offset);
        vkCmdDraw(frame.commandBuffer, batch.count, 1, 0, 0);
    }
}
}
