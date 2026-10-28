#include <client/crosshair/crosshair.hh>
#include <spear/rendering/vulkan/frame_context.hh>
#include <stdexcept>

namespace nx3d::client
{
namespace
{
void check(VkResult result)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error("Crosshair: Vulkan resource creation failed");
}
} // namespace
Crosshair::Crosshair(VkDevice device, VkPhysicalDevice physDevice, VkCommandPool commandPool,
                     VkQueue graphicsQueue, VkDescriptorPool descriptorPool,
                     VkDescriptorSetLayout descriptorSetLayout)
    : m_device(device), m_physDevice(physDevice), m_commandPool(commandPool),
      m_graphicsQueue(graphicsQueue), m_descriptorPool(descriptorPool)
{
    const VkDescriptorSetLayout layouts[] = {descriptorSetLayout, descriptorSetLayout};
    VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    sets.descriptorPool = descriptorPool;
    sets.descriptorSetCount = 2;
    sets.pSetLayouts = layouts;
    check(vkAllocateDescriptorSets(device, &sets, m_descriptorSets.data()));
    setSettings(m_settings);
}
Crosshair::~Crosshair()
{
    vkDeviceWaitIdle(m_device);
    vkFreeDescriptorSets(m_device, m_descriptorPool, 2, m_descriptorSets.data());
    for (const auto& [command, frame] : m_frames)
    {
        vkUnmapMemory(m_device, frame.memory);
        vkDestroyBuffer(m_device, frame.buffer, nullptr);
        vkFreeMemory(m_device, frame.memory, nullptr);
    }
}
void Crosshair::setSettings(const CrosshairSettings& settings)
{
    auto next = settings;
    next.sanitize();
    const bool colorChanged = !m_textures[0] || next.red != m_settings.red ||
                              next.green != m_settings.green || next.blue != m_settings.blue || next.alpha != m_settings.alpha;
    m_settings = next;
    if (!colorChanged)
        return;
    // Called between frames; finish draws before replacing textures/descriptors.
    // Color textures are replaced only here, outside the render pass.
    vkDeviceWaitIdle(m_device);
    for (int i = 0; i < 2; ++i)
    {
        const unsigned char pixel[] = {
                static_cast<unsigned char>(i ? 0 : next.red),
                static_cast<unsigned char>(i ? 0 : next.green),
                static_cast<unsigned char>(i ? 0 : next.blue),
                static_cast<unsigned char>(next.alpha)};
        auto texture = std::make_shared<spear::rendering::vulkan::STBTexture>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue);
        texture->loadFromRGBA(pixel, 1, 1);
        VkDescriptorImageInfo image{texture->getSampler(), texture->getImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_descriptorSets[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &image;
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
        m_textures[i] = std::move(texture);
    }
}
Crosshair::FrameBuffer& Crosshair::frameBuffer(VkCommandBuffer commandBuffer)
{
    auto& frame = m_frames[commandBuffer];
    if (frame.buffer)
        return frame;
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = 120 * 5 * sizeof(float); // two previews, ten rectangles each
    buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    check(vkCreateBuffer(m_device, &buffer, nullptr, &frame.buffer));
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(m_device, frame.buffer, &requirements);
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(m_physDevice, &properties);
    uint32_t memoryType = properties.memoryTypeCount;
    constexpr auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & flags) == flags)
        {
            memoryType = i;
            break;
        }
    if (memoryType == properties.memoryTypeCount)
        throw std::runtime_error("Crosshair: no host-visible memory");
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    check(vkAllocateMemory(m_device, &allocation, nullptr, &frame.memory));
    check(vkBindBufferMemory(m_device, frame.buffer, frame.memory, 0));
    check(vkMapMemory(m_device, frame.memory, 0, buffer.size, 0, &frame.mapped));
    return frame;
}
void Crosshair::render(int width, int height, float centerX, float centerY,
                       float spread, float zoom, int slot, std::array<float, 4> clip)
{
    const auto& context = spear::rendering::vulkan::g_frameContext;
    if (width <= 0 || height <= 0 || !context.commandBuffer || !context.uiPipeline || !context.uiPipelineLayout)
        return;
    const auto geometry = crosshairGeometry(m_settings, static_cast<int>(height * zoom), spread);
    struct Vertex
    {
        float x, y, z, u, v;
    };
    auto& frame = frameBuffer(context.commandBuffer);
    auto* vertices = static_cast<Vertex*>(frame.mapped) + std::clamp(slot, 0, 1) * 60;
    int count = 0;
    auto append = [&](float border)
    {
        for (int i = 0; i < geometry.count; ++i)
        {
            const auto& r = geometry.rects[i];
            const float left = std::max(clip[0], centerX + (r.x - border) * 2 / width);
            const float right = std::min(clip[2], centerX + (r.x + r.width + border) * 2 / width);
            const float top = std::min(clip[3], centerY - (r.y - border) * 2 / height);
            const float bottom = std::max(clip[1], centerY - (r.y + r.height + border) * 2 / height);
            if (right <= left || top <= bottom)
                continue;
            vertices[count++] = {left, top, 0, 0, 0};
            vertices[count++] = {right, top, 0, 1, 0};
            vertices[count++] = {right, bottom, 0, 1, 1};
            vertices[count++] = {left, top, 0, 0, 0};
            vertices[count++] = {right, bottom, 0, 1, 1};
            vertices[count++] = {left, bottom, 0, 0, 1};
        }
    };
    if (geometry.outline > 0)
        append(geometry.outline);
    const int outlineCount = count;
    append(0);
    vkCmdBindPipeline(context.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.uiPipeline);
    const VkDeviceSize offset = std::clamp(slot, 0, 1) * 60 * sizeof(Vertex);
    vkCmdBindVertexBuffers(context.commandBuffer, 0, 1, &frame.buffer, &offset);
    auto draw = [&](int texture, int first, int vertexCount)
    {
        vkCmdBindDescriptorSets(context.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                context.uiPipelineLayout, 0, 1, &m_descriptorSets[texture], 0, nullptr);
        vkCmdDraw(context.commandBuffer, vertexCount, 1, first, 0);
    };
    if (outlineCount)
        draw(1, 0, outlineCount);
    draw(0, outlineCount, count - outlineCount);
}
} // namespace nx3d::client
