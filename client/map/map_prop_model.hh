#ifndef NX3D_MAP_PROP_MODEL_HH
#define NX3D_MAP_PROP_MODEL_HH
#include <shared/map_document.hh>
#include <spear/rendering/vulkan/model/obj_model.hh>
namespace nx3d::client
{
std::shared_ptr<spear::rendering::vulkan::OBJModel> createMapPropModel(
        const MapProp& prop, VkDevice device, VkPhysicalDevice physicalDevice,
        VkCommandPool commands, VkQueue queue, VkDescriptorPool pool, VkDescriptorSetLayout layout);
void transformMapProp(spear::rendering::vulkan::OBJModel& model, const MapProp& prop);
} // namespace nx3d::client
#endif
