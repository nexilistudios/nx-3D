#ifndef NX3D_BRUSH_MAP_HH
#define NX3D_BRUSH_MAP_HH

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <cmath>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace nx3d
{
inline constexpr const char* kGeneratedMapPath = "maps/dust2_cs2/map.brush";
inline constexpr std::array<const char*, 14> kMapMaterials{{
        "sand", "paving", "plaster", "whitewash", "stone", "blue",
        "wood", "crate", "metal", "roof", "dark", "site_a", "site_b", "road"}};

struct MapBrush
{
    glm::vec3 center;
    glm::vec3 halfSize;
    std::string material = "stone";
    float slopeX = 0.0f;
    float slopeZ = 0.0f;
    float yaw = 0.0f;
    std::string group = "custom";
    bool operator==(const MapBrush&) const = default;
};

struct MapVertex
{
    glm::vec3 position;
    glm::vec2 uv;
};

// One deformed cube: its bottom is flat and the four top corners define a ramp.
// Yaw rotates around Y. Rendering and both physics worlds use these exact vertices.
inline std::array<MapVertex, 36> brushVertices(const MapBrush& brush)
{
    std::array<glm::vec3, 8> corners;
    const float angle = brush.yaw * 0.017453292519943295f;
    const float c = std::cos(angle), s = std::sin(angle);
    for (int i = 0; i < 8; ++i)
    {
        const float x = (i & 1) ? brush.halfSize.x : -brush.halfSize.x;
        const float z = (i & 2) ? brush.halfSize.z : -brush.halfSize.z;
        const float y = (i & 4) ? brush.halfSize.y +
                                  brush.slopeX * x / brush.halfSize.x +
                                  brush.slopeZ * z / brush.halfSize.z : -brush.halfSize.y;
        corners[i] = brush.center + glm::vec3(c * x + s * z, y, -s * x + c * z);
    }
    constexpr std::array<std::array<int, 4>, 6> faces{{
            {{0, 1, 3, 2}}, {{4, 6, 7, 5}}, {{0, 4, 5, 1}},
            {{2, 3, 7, 6}}, {{0, 2, 6, 4}}, {{1, 5, 7, 3}}}};
    std::array<MapVertex, 36> result;
    std::size_t next = 0;
    for (std::size_t face = 0; face < faces.size(); ++face)
        for (const int index : {0, 1, 2, 0, 2, 3})
        {
            const auto& p = corners[faces[face][index]];
            const auto uv = face < 2 ? glm::vec2(p.x, p.z) :
                            face < 4 ? glm::vec2(p.x, -p.y) : glm::vec2(p.z, -p.y);
            result[next++] = {p, uv / 128.0f};
        }
    return result;
}

// Shared by the client and authoritative server. Coordinates use engine axes:
// X/Z are horizontal and Y is up. Sizes are half extents, as in Bullet.
inline std::vector<MapBrush> readBrushMap(std::istream& input)
{
    std::string line;
    if (!std::getline(input, line)) throw std::runtime_error("Empty brush map");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const bool version2 = line == "nx3d-brush-map 2";
    if (!version2 && line != "nx3d-brush-map 1")
        throw std::runtime_error("Invalid brush map header");
    std::vector<MapBrush> brushes;
    std::size_t lineNumber = 1;
    while (std::getline(input, line))
    {
        ++lineNumber;
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;
        std::istringstream stream(line);
        std::string tag, extra;
        MapBrush brush{};
        if (!(stream >> tag) || tag != "brush" ||
            !(stream >> brush.center.x >> brush.center.y >> brush.center.z >>
                      brush.halfSize.x >> brush.halfSize.y >> brush.halfSize.z) ||
            (version2 && !(stream >> brush.material >> brush.slopeX >> brush.slopeZ >>
                                     brush.yaw >> brush.group)) ||
            (stream >> extra))
            throw std::runtime_error("Invalid brush at line " + std::to_string(lineNumber));
        const auto valid = [](float value) { return std::isfinite(value) && std::abs(value) <= 100000.0f; };
        if (!valid(brush.center.x) || !valid(brush.center.y) || !valid(brush.center.z) ||
            !valid(brush.halfSize.x) || !valid(brush.halfSize.y) || !valid(brush.halfSize.z) ||
            brush.halfSize.x <= 0 || brush.halfSize.y <= 0 || brush.halfSize.z <= 0 ||
            !valid(brush.slopeX) || !valid(brush.slopeZ) || !valid(brush.yaw) ||
            std::abs(brush.slopeX) + std::abs(brush.slopeZ) >= 2 * brush.halfSize.y)
            throw std::runtime_error("Invalid brush dimensions at line " + std::to_string(lineNumber));
        bool knownMaterial = false;
        for (const auto* name : kMapMaterials)
            if (brush.material == name) knownMaterial = true;
        if (!knownMaterial) throw std::runtime_error("Unknown map material: " + brush.material);
        brushes.push_back(brush);
        if (brushes.size() > 32768)
            throw std::runtime_error("Brush map exceeds 32768 brushes");
    }
    return brushes;
}
} // namespace nx3d

#endif
