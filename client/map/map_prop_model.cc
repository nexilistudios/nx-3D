#include <client/map/map_prop_model.hh>
#include <client/project_root.hh>
#include <fstream>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace nx3d::client
{
void transformMapProp(spear::rendering::vulkan::OBJModel& model, const MapProp& prop)
{
    glm::vec3 minimum(INFINITY), maximum(-INFINITY);
    for (const auto& vertex : model.getLoader().getVertices())
    {
        const glm::vec3 point(vertex.x, vertex.y, vertex.z);
        minimum = glm::min(minimum, point);
        maximum = glm::max(maximum, point);
    }
    const auto dimensions = maximum - minimum;
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(dimensions[axis]) || dimensions[axis] <= 0.00001f)
            throw std::runtime_error("Model needs nonzero dimensions on each axis: " + prop.model);
    auto transform = glm::translate(glm::mat4(1), prop.center);
    transform = glm::rotate(transform, glm::radians(prop.yaw), {0, 1, 0});
    transform = glm::scale(transform, prop.halfSize * 2.0f / dimensions);
    model.setModel(glm::translate(transform, -(minimum + maximum) * 0.5f));
}
std::shared_ptr<spear::rendering::vulkan::OBJModel> createMapPropModel(
        const MapProp& prop, VkDevice device, VkPhysicalDevice physicalDevice,
        VkCommandPool commands, VkQueue queue, VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
    validateMapProp(prop);
    const auto obj = std::filesystem::path(projectAssetPath(prop.model));
    std::ifstream source(obj);
    if (!source)
        throw std::runtime_error("Missing model: " + obj.string());
    auto material = obj;
    material.replace_extension(".mtl");
    std::string line;
    while (std::getline(source, line))
    {
        std::istringstream row(line);
        std::string tag, name;
        if (row >> tag && tag == "mtllib" && std::getline(row >> std::ws, name))
        {
            name.erase(name.find_last_not_of(" \t\r") + 1);
            material = obj.parent_path() / name;
            break;
        }
    }
    auto model = std::make_shared<spear::rendering::vulkan::OBJModel>(
            device, physicalDevice, commands, queue, obj.string(),
            std::filesystem::exists(material) ? material.string() : std::string{}, pool, layout,
            spear::physics::bullet::ObjectData({}, 0, glm::vec3(0), glm::vec3(1)));
    if (model->getLoader().getVertices().empty() || model->getLoader().getFaces().empty())
        throw std::runtime_error("Model has no renderable faces: " + prop.model);
    transformMapProp(*model, prop);
    return model;
}
} // namespace nx3d::client
