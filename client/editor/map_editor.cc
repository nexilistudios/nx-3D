#include <client/app/client_app.hh>
#include <client/editor/map_editor.hh>
#include <client/project_root.hh>
#include <shared/player_rules.hh>
#include <glm/gtc/quaternion.hpp>
#include <spear/rendering/vulkan/frame_context.hh>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace nx3d::client::editor
{
namespace vk = spear::rendering::vulkan;
namespace
{
void line(vk::Cube& cube, glm::vec3 a, glm::vec3 b, float width)
{
    const auto delta = b - a;
    const float length = glm::length(delta);
    if (length < 0.001f)
    {
        cube.setModel(glm::mat4(0));
        return;
    }
    const auto direction = delta / length;
    const auto axis = glm::cross(glm::vec3(0, 0, 1), direction);
    auto transform = glm::translate(glm::mat4(1), (a + b) * 0.5f);
    const float angle = std::acos(std::clamp(direction.z, -1.0f, 1.0f));
    if (angle > .00001f)
        transform = glm::rotate(transform, angle, glm::length(axis) > .00001f ? glm::normalize(axis) : glm::vec3(0, 1, 0));
    cube.setModel(glm::scale(transform, {width, width, length * 0.5f}));
}
std::string vectorText(glm::vec3 point)
{
    std::ostringstream text;
    text << point.x << ", " << point.y << ", " << point.z;
    return text.str();
}
} // namespace
MapEditor::MapEditor(ClientApp& application) : document(application.mapDocument), app(application)
{
    const auto assets = std::filesystem::path(projectAssetPath("props"));
    if (std::filesystem::exists(assets))
        for (const auto& entry : std::filesystem::recursive_directory_iterator(assets))
            if (entry.is_regular_file() && entry.path().extension() == ".obj")
                models.push_back(std::filesystem::relative(entry.path(), projectAssetPath()).generic_string());
    std::sort(models.begin(), models.end());
    app.ui->clear();
    const auto font = projectAssetPath("fonts/firacode/static/FiraCode-Retina.ttf");
    for (int i = 0; i < 30; ++i)
    {
        auto text = std::make_unique<spear::ui::vulkan::Text>(
                app.renderer.getDevice(), app.renderer.getPhysicalDevice(), app.renderer.getCommandPool(),
                app.renderer.getGraphicsQueue(), app.descriptorPool, app.descriptorSetLayout, font, i == 0 ? 22 : 15);
        text->setPosition(i < 22 ? glm::vec2(-0.96f, 0.90f - i * 0.057f)
                                 : glm::vec2(0.44f, 0.90f - (i - 22) * 0.057f));
        text->setColor(i == 0 ? SDL_Color{255, 214, 140, 255} : SDL_Color{221, 232, 242, 255});
        app.ui->registerText(*text);
        texts.push_back(std::move(text));
    }
    auto panel = [&](glm::vec2 position, glm::vec2 size)
    {
        auto texture = std::make_shared<vk::STBTexture>(app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                                                        app.renderer.getCommandPool(), app.renderer.getGraphicsQueue());
        const unsigned char pixel[]{15, 23, 33, 225};
        texture->loadFromRGBA(pixel, 1, 1);
        auto quad = std::make_shared<spear::ui::vulkan::Quad2D>(app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                                                                app.descriptorPool, app.descriptorSetLayout, texture);
        quad->setPosition(position);
        quad->setSize(size);
        panels.push_back(quad);
    };
    panel({-1, -0.38f}, {0.64f, 1.38f});
    panel({0.42f, 0.43f}, {0.58f, 0.57f});
    auto scene = app.scene_manager.getSceneById(app.game_scene_id);
    for (int i = 0; i < 28; ++i)
    {
        auto cube = std::make_shared<vk::Cube>(app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                                               spear::physics::bullet::ObjectData({}, 0, glm::vec3(0), glm::vec3(1)), i < 24 ? glm::vec4(1, .8f, .1f, 1) : glm::vec4(.1f, .9f, 1, 1));
        cube->setEmissive(false);
        cube->setCastsShadow(false);
        cube->setModel(glm::mat4(0));
        scene->addObject(cube);
        edges.push_back(cube);
    }
    for (int i = 0; i < 34; ++i)
    {
        auto cube = std::make_shared<vk::Cube>(app.renderer.getDevice(), app.renderer.getPhysicalDevice(),
                                               spear::physics::bullet::ObjectData({}, 0, glm::vec3(0), glm::vec3(1)), glm::vec4(.25f, .5f, .6f, 1));
        cube->setEmissive(false);
        cube->setCastsShadow(false);
        scene->addObject(cube);
        gridLines.push_back(cube);
    }
    app.ui->renderer().setOverlayCallback([this]
                                          { renderHud(); });
    if (!document.data.brushes.empty())
    {
        const auto floor = std::find_if(document.data.brushes.begin(), document.data.brushes.end(),
            [](const MapBrush& brush) { return brush.group.ends_with("/floor"); });
        const auto& start = floor != document.data.brushes.end() ? *floor : document.data.brushes.front();
        app.camera.setPosition({start.center.x, start.center.y + start.halfSize.y + kStandingEyeHeight,
                                start.center.z});
    }
    fly(false);
    refresh();
    selectionOutline();
}
MapEditor::~MapEditor()
{
    app.ui->renderer().setOverlayCallback(nullptr);
    auto scene = app.scene_manager.getSceneById(app.game_scene_id);
    for (const auto& edge : edges)
        scene->removeObject(edge->getId());
    for (const auto& grid : gridLines)
        scene->removeObject(grid->getId());
    vkDeviceWaitIdle(app.renderer.getDevice());
}
void MapEditor::fly(bool enabled)
{
    flying = enabled;
    dragging = false;
    dragStarted = false;
    keys.clear();
    SDL_SetWindowRelativeMouseMode(app.window.getSDLWindow(), enabled);
    refresh();
}
glm::vec3 MapEditor::ray() const
{
    if (flying)
        return app.camera.getFront();
    int width = 0, height = 0;
    SDL_GetWindowSize(app.window.getSDLWindow(), &width, &height);
    const float x = mouse.x / std::max(width, 1) * 2 - 1;
    const float y = 1 - mouse.y / std::max(height, 1) * 2;
    const float tangent = std::tan(glm::radians(app.camera.getFov()) * .5f);
    return glm::normalize(app.camera.getFront() + app.camera.getRight() * (x * tangent * width / std::max(height, 1)) + app.camera.getUp() * (y * tangent));
}
glm::vec3 MapEditor::dragIntersection(glm::vec3 direction) const
{
    const auto origin = app.camera.getPosition();
    if (std::abs(direction.y) < .001f)
        return dragPoint;
    const float distance = (dragCenter.y - origin.y) / direction.y;
    return distance > 0 && distance < 20000 ? origin + direction * distance : dragPoint;
}
bool MapEditor::save()
{
    try
    {
        saveMapDocument(app.mapFilePath, document.data);
        document.markSaved();
        notice = "Saved " + std::filesystem::path(app.mapFilePath).filename().string() + ". Backup: .bak";
        refresh();
        return true;
    }
    catch (const std::exception& error)
    {
        notice = error.what();
        refresh();
        return false;
    }
}
bool MapEditor::requestQuit()
{
    if (!document.dirty() && std::filesystem::exists(app.mapFilePath))
        return true;
    fly(false);
    const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Save"}, {0, 2, "Discard"}, {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"}};
    const SDL_MessageBoxData dialog{SDL_MESSAGEBOX_WARNING, app.window.getSDLWindow(), "Unsaved map",
                                    "Save your 3D map changes before closing?", 3, buttons, nullptr};
    int choice = 0;
    if (!SDL_ShowMessageBox(&dialog, &choice))
        return false;
    return choice == 2 || (choice == 1 && save());
}
void MapEditor::action(int row, bool reverse)
{
    const float sign = reverse ? -1.0f : 1.0f;
    int width = 0, height = 0;
    SDL_GetWindowSize(app.window.getSDLWindow(), &width, &height);
    const auto direction = !flying && mouse.x < width * .32f ? app.camera.getFront() : ray();
    const auto placement = document.placement(app.camera.getPosition(), direction);
    switch (row)
    {
        case 2:
            fly(!flying);
            return;
        case 16:
            freeFlight = !freeFlight;
            app.movement_controller.resetVerticalMotion();
            notice = freeFlight ? "Free flight: WASD, Space/C up/down. G returns to walking." :
                                  "Walk mode: WASD moves through the map; Space jumps.";
            break;
        case 3:
            document.addBlock(placement);
            changed = true;
            notice = "Block created. Drag to move; wheel pushes or pulls the selected face.";
            break;
        case 4:
            if (models.empty())
            {
                notice = "Add OBJ + MTL + textures under assets/props/, then reopen the editor.";
                break;
            }
            document.addProp(models[modelIndex], placement);
            changed = true;
            notice = "Model placed. C toggles its box collision.";
            break;
        case 5:
            document.duplicate();
            changed = true;
            break;
        case 6:
            document.erase();
            changed = true;
            break;
        case 7:
            save();
            break;
        case 8:
            changed = document.undo() || changed;
            break;
        case 9:
            changed = document.redo() || changed;
            break;
        case 10:
            document.material = (document.material + static_cast<int>(sign) + kMapMaterials.size()) % kMapMaterials.size();
            document.paint();
            changed = true;
            break;
        case 11:
            if (!models.empty())
                modelIndex = (modelIndex + (reverse ? models.size() - 1 : 1)) % models.size();
            break;
        case 12:
            document.grid = std::clamp(document.grid * (reverse ? .5f : 2.0f), 1.0f, 256.0f);
            break;
        case 13:
            document.rotate(sign * 15);
            changed = true;
            break;
        case 14:
            document.toggleSolid();
            changed = true;
            break;
        case 15:
            if (const auto bounds = document.selectedBounds())
                app.camera.setPosition(bounds->center - app.camera.getFront() * std::max(160.0f, glm::length(bounds->halfSize) * 4));
            break;
        default:
            break;
    }
    refresh();
    selectionOutline();
}
bool MapEditor::handle(const SDL_Event& event)
{
    try
    {
        if (event.type == SDL_EVENT_QUIT)
            return requestQuit();
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            fly(false);
            momentaryFly = false;
        }
        if (event.type == SDL_EVENT_WINDOW_RESIZED)
        {
            app.window.resize();
            const auto size = app.window.getSize();
            app.renderer.setViewPort(size.x, size.y);
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            mouse = {event.motion.x, event.motion.y};
            if (flying)
                app.camera.rotate(event.motion.xrel, event.motion.yrel);
            else if (dragging && document.selection)
            {
                auto delta = document.snap(dragIntersection(ray()) - dragPoint);
                delta.y = 0;
                if (SDL_GetModState() & SDL_KMOD_SHIFT)
                    delta = {0, std::round((dragMouse.y - mouse.y) / 20) * document.grid, 0};
                const auto next = dragCenter + delta;
                auto bounds = *document.selectedBounds();
                if (next != bounds.center && glm::all(glm::lessThanEqual(glm::abs(next), glm::vec3(100000))))
                {
                    if (!dragStarted)
                    {
                        document.checkpoint();
                        dragStarted = true;
                    }
                    if (document.selection->prop)
                        document.data.props[document.selection->index].center = next;
                    else
                        document.data.brushes[document.selection->index].center = next;
                    changed = true;
                    selectionOutline();
                    refresh();
                }
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            if (event.button.button == SDL_BUTTON_RIGHT && momentaryFly)
            {
                fly(false);
                momentaryFly = false;
            }
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                dragging = false;
                dragStarted = false;
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            mouse = {event.button.x, event.button.y};
            if (event.button.button == SDL_BUTTON_RIGHT && !flying)
            {
                momentaryFly = true;
                fly(true);
            }
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                int w = 0, h = 0;
                SDL_GetWindowSize(app.window.getSDLWindow(), &w, &h);
                const float x = mouse.x * 2 / std::max(w, 1) - 1;
                const float y = 1 - mouse.y * 2 / std::max(h, 1);
                if (!flying && x < -.36f && y > -.38f)
                {
                    const int row = static_cast<int>(std::round((.92f - y) / .057f));
                    action(row);
                    return false;
                }
                if (!flying && x > .42f && y > .43f)
                    return false;
                document.selection = pick(document.data, app.camera.getPosition(), ray());
                if (document.selection && !flying)
                {
                    dragging = true;
                    dragStarted = false;
                    dragMouse = mouse;
                    dragCenter = document.selectedBounds()->center;
                    dragPoint = dragIntersection(ray());
                }
                selectionOutline();
                refresh();
            }
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
            if (std::abs(direction) < .001f)
                return false;
            if (document.resizeFace((direction > 0 ? 1 : -1) * document.grid))
                changed = true;
            selectionOutline();
            refresh();
        }
        if (event.type == SDL_EVENT_KEY_UP)
            keys[event.key.key] = false;
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            keys[event.key.key] = true;
            const bool control = event.key.mod & SDL_KMOD_CTRL;
            const bool shift = event.key.mod & SDL_KMOD_SHIFT;
            const auto key = event.key.key;
            if (key == SDLK_ESCAPE && !event.key.repeat)
            {
                if (flying)
                {
                    fly(false);
                    momentaryFly = false;
                }
                else
                    return requestQuit();
            }
            else if (key == SDLK_TAB && !event.key.repeat)
                action(2);
            else if (key == SDLK_G && !event.key.repeat)
                action(16);
            else if (control && key == SDLK_S && !event.key.repeat)
                action(7);
            else if (control && key == SDLK_Z)
                action(shift ? 9 : 8);
            else if (control && key == SDLK_Y)
                action(9);
            else if (control && key == SDLK_D && !event.key.repeat)
                action(5);
            else if (key == SDLK_B && !event.key.repeat)
                action(3);
            else if (key == SDLK_O && !event.key.repeat)
                action(4);
            else if (key == SDLK_DELETE || key == SDLK_BACKSPACE)
                action(6);
            else if (key == SDLK_M)
                action(10, shift);
            else if (key == SDLK_LEFTBRACKET || key == SDLK_RIGHTBRACKET)
                action(11, key == SDLK_LEFTBRACKET);
            else if (key == SDLK_MINUS || key == SDLK_EQUALS || key == SDLK_PLUS)
                action(12, key == SDLK_MINUS);
            else if (key == SDLK_R)
                action(13, shift);
            else if (key == SDLK_C && !flying && !event.key.repeat)
                action(14);
            else if (key == SDLK_F && !event.key.repeat)
                action(15);
            else
            {
                glm::vec3 delta(0);
                if (key == SDLK_LEFT)
                    delta.x -= document.grid;
                if (key == SDLK_RIGHT)
                    delta.x += document.grid;
                if (key == SDLK_UP)
                    delta.z -= document.grid;
                if (key == SDLK_DOWN)
                    delta.z += document.grid;
                if (key == SDLK_PAGEUP)
                    delta.y += document.grid;
                if (key == SDLK_PAGEDOWN)
                    delta.y -= document.grid;
                if (delta != glm::vec3(0))
                {
                    document.move(delta);
                    changed = true;
                    selectionOutline();
                    refresh();
                }
            }
        }
    }
    catch (const std::exception& error)
    {
        notice = error.what();
        refresh();
    }
    return false;
}
void MapEditor::update(float dt)
{
    if (flying && freeFlight)
    {
        const float amount = std::min(dt, .1f) * ((SDL_GetModState() & SDL_KMOD_SHIFT) ? 4 : 1);
        if (keys[SDLK_W])
            app.camera.moveForward(amount);
        if (keys[SDLK_S] && !(SDL_GetModState() & SDL_KMOD_CTRL))
            app.camera.moveBackward(amount);
        if (keys[SDLK_A])
            app.camera.moveLeft(amount);
        if (keys[SDLK_D] && !(SDL_GetModState() & SDL_KMOD_CTRL))
            app.camera.moveRight(amount);
        if (keys[SDLK_SPACE])
            app.camera.moveUp(amount);
        if (keys[SDLK_C])
            app.camera.moveDown(amount);
    }
    else if (flying)
    {
        const std::unordered_map<SDL_Keycode, bool> movement{
            {SDLK_W, keys[SDLK_W]}, {SDLK_S, keys[SDLK_S] && !keys[SDLK_LCTRL] && !keys[SDLK_RCTRL]},
            {SDLK_A, keys[SDLK_A]}, {SDLK_D, keys[SDLK_D]}, {SDLK_SPACE, keys[SDLK_SPACE]}};
        app.movement_controller.processInput(movement, std::min(dt, .1f));
    }
    if (changed)
    {
        app.mapDocument = document.data;
        try
        {
            app.rebuildMapVisuals();
        }
        catch (const std::exception& error)
        {
            notice = error.what();
            document.undo();
            app.mapDocument = document.data;
            app.rebuildMapVisuals();
        }
        changed = false;
        selectionOutline();
        refresh();
    }
}
void MapEditor::selectionOutline()
{
    for (auto& edge : edges)
        edge->setModel(glm::mat4(0));
    if (const auto brush = document.selectedBounds())
    {
        const auto vertices = brushVertices(*brush);
        const float width = std::clamp(glm::distance(app.camera.getPosition(), brush->center) * .001f, .5f, 3.0f);
        for (int face = 0; face < 6; ++face)
        {
            const int start = face * 6;
            const std::array<glm::vec3, 4> corners{vertices[start].position, vertices[start + 1].position,
                                                   vertices[start + 2].position, vertices[start + 5].position};
            for (int i = 0; i < 4; ++i)
            {
                line(*edges[face * 4 + i], corners[i], corners[(i + 1) % 4], width);
                if (document.selection->face == face)
                    line(*edges[24 + i], corners[i], corners[(i + 1) % 4], width * 1.5f);
            }
        }
    }
    const auto origin = document.selectedBounds() ? document.selectedBounds()->center : glm::vec3(0);
    const float y = document.selectedBounds() ? origin.y - document.selectedBounds()->halfSize.y + .3f : .3f;
    for (int i = 0; i < 17; ++i)
    {
        const float offset = (i - 8) * document.grid, reach = 8 * document.grid;
        line(*gridLines[i * 2], {origin.x + offset, y, origin.z - reach}, {origin.x + offset, y, origin.z + reach}, .30f);
        line(*gridLines[i * 2 + 1], {origin.x - reach, y, origin.z + offset}, {origin.x + reach, y, origin.z + offset}, .30f);
    }
}
void MapEditor::refresh()
{
    if (texts.empty())
        return;
    std::array<std::string, 30> labels{};
    labels[0] = "3D MAP EDITOR";
    labels[1] = std::string(document.dirty() ? "* " : "") + std::filesystem::path(app.mapFilePath).filename().string();
    labels[2] = flying ? "[ TAB: Release mouse ]" : "[ TAB: Look / walk ]";
    labels[3] = "[ B: Add block ]";
    labels[4] = "[ O: Add OBJ model ]";
    labels[5] = "[ Ctrl+D: Duplicate ]";
    labels[6] = "[ Delete: Remove ]";
    labels[7] = "[ Ctrl+S: Save map ]";
    labels[8] = "[ Ctrl+Z: Undo ]";
    labels[9] = "[ Ctrl+Y: Redo ]";
    labels[10] = "M: Material " + std::string(kMapMaterials[document.material]);
    labels[11] = "[ ]: " + (models.empty() ? std::string("No models") : std::filesystem::path(models[modelIndex]).stem().string());
    labels[12] = "-/+: Grid " + std::to_string(static_cast<int>(document.grid));
    labels[13] = "[ R: Rotate 15 degrees ]";
    labels[14] = "[ C: Model collision ]";
    labels[15] = "[ F: Focus selection ]";
    labels[16] = "Click a face; wheel: resize";
    labels[17] = "Drag: move; Shift: vertical";
    labels[18] = "Arrows: X/Z; PgUp/Dn: Y";
    labels[19] = "RMB + WASD: walk / look";
    labels[20] = freeFlight ? "G: walk; Space/C: up/down" : "G: free flight; Space: jump";
    labels[21] = "Esc: release mouse / close";
    labels[22] = "SELECTION";
    if (document.selection)
    {
        const auto bounds = *document.selectedBounds();
        labels[23] = document.selection->prop ? "OBJ model" : "Block / face " + std::to_string(document.selection->face + 1);
        labels[24] = "Pos " + vectorText(bounds.center);
        labels[25] = "Size " + vectorText(bounds.halfSize * 2.0f);
        labels[26] = "Yaw " + std::to_string(static_cast<int>(bounds.yaw));
        labels[27] = document.selection->prop ? (document.data.props[document.selection->index].solid ? "Collision: box" : "Collision: off") : bounds.material;
    }
    else
        labels[23] = "Click a block or model";
    labels[28] = std::to_string(document.data.brushes.size()) + " blocks, " + std::to_string(document.data.props.size()) + " models";
    // A compact notice at the bottom avoids covering the editing viewport.
    labels[29] = notice;
    for (std::size_t i = 0; i < labels.size(); ++i)
    {
        if (i != 29 && labels[i].size() > 31)
            labels[i] = labels[i].substr(0, 28) + "...";
        if (i == 29 && labels[i].size() > 85)
            labels[i] = labels[i].substr(0, 82) + "...";
        if (texts[i]->getString() != labels[i])
            texts[i]->setString(labels[i]);
    }
}
void MapEditor::renderHud()
{
    const auto context = spear::ui::RenderContext{spear::rendering::vulkan::g_frameContext.commandBuffer};
    for (const auto& panel : panels)
        panel->render(context);
    if (flying)
    {
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(app.window.getSDLWindow(), &width, &height);
        app.crosshair->render(width, height);
    }
}
} // namespace nx3d::client::editor
