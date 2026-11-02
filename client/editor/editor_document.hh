#ifndef NX3D_EDITOR_DOCUMENT_HH
#define NX3D_EDITOR_DOCUMENT_HH

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <shared/map_document.hh>

namespace nx3d::client::editor
{
enum class Shape { cube, sphere, wall, door };
inline const char* shapeName(Shape shape)
{
    switch (shape)
    {
        case Shape::cube: return "Cube";
        case Shape::sphere: return "Sphere";
        case Shape::wall: return "2D wall";
        case Shape::door: return "Door";
    }
    return "Cube";
}

struct Selection
{
    bool prop = false;
    std::size_t index = 0;
    int face = 1;
    float distance = INFINITY;
    glm::vec3 point{0}, normal{0, 1, 0};
};
inline std::optional<Selection> pick(const MapDocument& document, glm::vec3 origin, glm::vec3 direction)
{
    std::optional<Selection> result;
    auto test = [&](const auto& vertices, bool prop, bool sphere, std::size_t index)
    {
        for (std::size_t i = 0; i < vertices.size(); i += 3)
        {
            const auto a = vertices[i].position, b = vertices[i + 1].position, c = vertices[i + 2].position;
            const auto edge1 = b - a, edge2 = c - a;
            const auto p = glm::cross(direction, edge2);
            const float determinant = glm::dot(edge1, p);
            if (std::abs(determinant) < 0.00001f)
                continue;
            const float inverse = 1 / determinant;
            const auto t = origin - a;
            const float u = glm::dot(t, p) * inverse;
            if (u < 0 || u > 1)
                continue;
            const auto q = glm::cross(t, edge1);
            const float v = glm::dot(direction, q) * inverse;
            if (v < 0 || u + v > 1)
                continue;
            const float distance = glm::dot(edge2, q) * inverse;
            if (distance < 0.01f || (result && distance >= result->distance))
                continue;
            result = Selection{prop, index, sphere ? 0 : static_cast<int>(i / 6),
                               distance, origin + direction * distance,
                               glm::normalize(glm::cross(edge1, edge2))};
        }
    };
    for (std::size_t i = 0; i < document.brushes.size(); ++i)
        test(brushVertices(document.brushes[i]), false, false, i);
    for (std::size_t i = 0; i < document.props.size(); ++i)
        if (isEditorSphere(document.props[i]))
            test(propCollisionVertices(document.props[i]), true, true, i);
        else
            test(brushVertices(propBounds(document.props[i])), true, false, i);
    return result;
}

class Document
{
public:
    explicit Document(MapDocument initial) : data(std::move(initial)), saved(data)
    {
    }
    MapDocument data;
    std::optional<Selection> selection;
    float grid = 32;
    int material = 4;
    bool dirty() const
    {
        return data != saved;
    }
    void markSaved()
    {
        saved = data;
    }
    glm::vec3 snap(glm::vec3 point) const
    {
        return glm::round(point / grid) * grid;
    }
    std::optional<MapBrush> selectedBounds() const
    {
        if (!selection)
            return {};
        return selection->prop ? propBounds(data.props.at(selection->index)) : data.brushes.at(selection->index);
    }
    void checkpoint()
    {
        undoStack.push_back(data);
        if (undoStack.size() > 64)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    bool undo()
    {
        if (undoStack.empty())
            return false;
        redoStack.push_back(data);
        data = std::move(undoStack.back());
        undoStack.pop_back();
        selection.reset();
        return true;
    }
    bool redo()
    {
        if (redoStack.empty())
            return false;
        undoStack.push_back(data);
        data = std::move(redoStack.back());
        redoStack.pop_back();
        selection.reset();
        return true;
    }
    glm::vec3 placement(glm::vec3 origin, glm::vec3 direction) const
    {
        const auto hit = pick(data, origin, direction);
        if (hit)
        {
            auto center = snap(hit->point);
            const auto normal = hit->normal;
            const float support = grid * .5f * (std::abs(normal.x) + std::abs(normal.y) + std::abs(normal.z));
            center += normal * (support - glm::dot(center - hit->point, normal));
            return center;
        }
        return snap(origin + direction * grid * 6.0f);
    }
    void addBlock(glm::vec3 center)
    {
        if (data.brushes.size() >= 32768)
            throw std::runtime_error("Brush limit reached");
        checkpoint();
        data.brushes.push_back({center, glm::vec3(grid * 0.5f), kMapMaterials[material], 0, 0, 0, "custom/block"});
        selection = Selection{false, data.brushes.size() - 1};
    }
    void addShape(Shape shape, glm::vec3 center)
    {
        if (shape == Shape::sphere)
        {
            addProp(kEditorSphereModel, center);
            return;
        }
        if (shape == Shape::cube)
        {
            addBlock(center);
            return;
        }
        if (data.brushes.size() >= 32768)
            throw std::runtime_error("Brush limit reached");
        const glm::vec3 size = shape == Shape::wall
                                       ? glm::vec3(grid * 2.0f, grid * 1.5f, 4.0f)
                                       : glm::vec3(grid, grid * 2.0f, 6.0f);
        center.y += size.y - grid * 0.5f;
        checkpoint();
        data.brushes.push_back({center, size,
                                shape == Shape::door ? "wood" : kMapMaterials[material], 0, 0, 0,
                                shape == Shape::wall ? "custom/wall" : "custom/door"});
        selection = Selection{false, data.brushes.size() - 1};
    }
    void addProp(const std::string& path, glm::vec3 center)
    {
        if (data.props.size() >= 4096)
            throw std::runtime_error("Model limit reached");
        MapProp prop{path, center, glm::vec3(grid * 0.5f)};
        validateMapProp(prop);
        checkpoint();
        data.props.push_back(prop);
        selection = Selection{true, data.props.size() - 1};
    }
    void erase()
    {
        if (!selection)
            return;
        checkpoint();
        if (selection->prop)
            data.props.erase(data.props.begin() + selection->index);
        else
            data.brushes.erase(data.brushes.begin() + selection->index);
        selection.reset();
    }
    void duplicate()
    {
        if (!selection)
            return;
        if ((selection->prop && data.props.size() >= 4096) || (!selection->prop && data.brushes.size() >= 32768))
            throw std::runtime_error("Map object limit reached");
        checkpoint();
        if (selection->prop)
        {
            auto copy = data.props[selection->index];
            copy.center.x += grid;
            data.props.push_back(copy);
            selection->index = data.props.size() - 1;
        }
        else
        {
            auto copy = data.brushes[selection->index];
            copy.center.x += grid;
            data.brushes.push_back(copy);
            selection->index = data.brushes.size() - 1;
        }
    }
    void move(glm::vec3 delta)
    {
        if (!selection)
            return;
        const auto bounds = *selectedBounds();
        for (int axis = 0; axis < 3; ++axis)
            if (std::abs(bounds.center[axis] + delta[axis]) > 100000)
                return;
        checkpoint();
        if (selection->prop)
            data.props[selection->index].center += delta;
        else
            data.brushes[selection->index].center += delta;
    }
    bool setSelectedField(int field, float value)
    {
        if (!selection || field < 0 || field > 5 || !std::isfinite(value))
            return false;
        auto bounds = *selectedBounds();
        if (field < 3)
            bounds.center[field] = value;
        else
            bounds.halfSize[field - 3] = value * .5f;
        for (int axis = 0; axis < 3; ++axis)
            if (std::abs(bounds.center[axis]) > 100000 || bounds.halfSize[axis] < .5f ||
                bounds.halfSize[axis] > 100000)
                return false;
        if (std::abs(bounds.slopeX) + std::abs(bounds.slopeZ) >= 2 * bounds.halfSize.y)
            return false;
        if (selection->prop)
        {
            auto next = data.props[selection->index];
            next.center = bounds.center;
            next.halfSize = bounds.halfSize;
            validateMapProp(next);
            if (next == data.props[selection->index]) return true;
            checkpoint();
            data.props[selection->index] = next;
        }
        else
        {
            if (bounds == data.brushes[selection->index]) return true;
            checkpoint();
            data.brushes[selection->index] = bounds;
        }
        return true;
    }
    bool resizeFace(float amount)
    {
        if (!selection)
            return false;
        auto b = *selectedBounds();
        const int face = selection->face;
        const int axis = face < 2 ? 1 : face < 4 ? 2
                                                 : 0;
        const float sign = face % 2 ? 1.0f : -1.0f;
        b.halfSize[axis] += amount * 0.5f;
        if (b.halfSize[axis] < 0.5f || b.halfSize[axis] > 100000 ||
            std::abs(b.slopeX) + std::abs(b.slopeZ) >= b.halfSize.y * 2)
            return false;
        glm::vec3 offset(0);
        offset[axis] = sign * amount * 0.5f;
        b.center += glm::vec3(glm::rotate(glm::mat4(1), glm::radians(b.yaw), glm::vec3(0, 1, 0)) * glm::vec4(offset, 0));
        for (int a = 0; a < 3; ++a)
            if (std::abs(b.center[a]) > 100000)
                return false;
        checkpoint();
        if (selection->prop)
        {
            auto& p = data.props[selection->index];
            p.center = b.center;
            p.halfSize = b.halfSize;
        }
        else
            data.brushes[selection->index] = b;
        return true;
    }
    void rotate(float degrees)
    {
        if (!selection)
            return;
        checkpoint();
        auto& yaw = selection->prop ? data.props[selection->index].yaw : data.brushes[selection->index].yaw;
        yaw = std::remainder(yaw + degrees, 360.0f);
    }
    void paint()
    {
        if (!selection || selection->prop)
            return;
        checkpoint();
        data.brushes[selection->index].material = kMapMaterials[material];
    }
    void toggleSolid()
    {
        if (!selection || !selection->prop)
            return;
        checkpoint();
        auto& p = data.props[selection->index];
        p.solid = !p.solid;
    }

private:
    MapDocument saved;
    std::vector<MapDocument> undoStack, redoStack;
};
} // namespace nx3d::client::editor
#endif
