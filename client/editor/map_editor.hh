#ifndef NX3D_MAP_EDITOR_HH
#define NX3D_MAP_EDITOR_HH
#include <client/editor/editor_document.hh>
#include <memory>
#include <spear/spear_vulkan.hh>
#include <unordered_map>

namespace nx3d::client
{
class ClientApp;
namespace editor
{
class MapEditor
{
public:
    explicit MapEditor(ClientApp& app);
    ~MapEditor();
    bool handle(const SDL_Event& event);
    void update(float dt);
    Document document;

private:
    ClientApp& app;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> texts;
    std::vector<std::shared_ptr<spear::ui::vulkan::Quad2D>> panels;
    std::vector<std::shared_ptr<spear::rendering::vulkan::Cube>> edges;
    std::vector<std::shared_ptr<spear::rendering::vulkan::Cube>> gridLines;
    std::vector<std::string> models;
    std::size_t modelIndex = 0;
    std::unordered_map<SDL_Keycode, bool> keys;
    bool flying = false, freeFlight = false, momentaryFly = false, changed = false, dragging = false, dragStarted = false;
    glm::vec2 mouse{0};
    glm::vec2 dragMouse{0};
    glm::vec3 dragPoint{0}, dragCenter{0};
    std::string notice = "Click a face to select it. B adds a block; O adds the selected model.";
    glm::vec3 ray() const;
    glm::vec3 dragIntersection(glm::vec3 direction) const;
    void fly(bool enabled);
    void action(int row, bool reverse = false);
    void refresh();
    void selectionOutline();
    void renderHud();
    bool save();
    bool requestQuit();
};
} // namespace editor
} // namespace nx3d::client
#endif
