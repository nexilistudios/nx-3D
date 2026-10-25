#include <shared/brush_map.hh>

#include <glm/geometric.hpp>
#include <cmath>
#include <sstream>
#include <stdexcept>

void require(bool condition) { if (!condition) throw std::runtime_error("Brush geometry regression"); }

int main()
{
    std::istringstream map("nx3d-brush-map 2\r\n# Deformed cube\n"
                           "brush 32 64 -96 16 32 64 stone 8 0 90 test/ramp\n");
    const auto brushes = nx3d::readBrushMap(map);
    require(brushes.size() == 1 && brushes[0].center.z == -96);
    require(brushes[0].halfSize.y == 32 && brushes[0].slopeX == 8);
    const auto vertices = nx3d::brushVertices(brushes[0]);
    float minY = 1000, maxY = -1000, minX = 1000, maxX = -1000;
    for (const auto& vertex : vertices)
    {
        minY = std::min(minY, vertex.position.y); maxY = std::max(maxY, vertex.position.y);
        minX = std::min(minX, vertex.position.x); maxX = std::max(maxX, vertex.position.x);
    }
    require(std::abs(minY - 32) < .001f && std::abs(maxY - 104) < .001f);
    require(std::abs(minX + 32) < .001f && std::abs(maxX - 96) < .001f);
    for (std::size_t i = 0; i < vertices.size(); i += 3)
    {
        const auto a = vertices[i].position, b = vertices[i+1].position, c = vertices[i+2].position;
        const auto normal = glm::cross(b-a,c-a);
        require(glm::dot(normal,(a+b+c)/3.0f - brushes[0].center) > 0);
    }

    for (const char* invalid : {"nx3d-brush-map 3\n", "nx3d-brush-map 1\nbrush 0 0 0 0 1 1\n",
                                "nx3d-brush-map 2\nbrush 0 0 0 1 1 1 stone 3 0 0 inverted\n",
                                "nx3d-brush-map 2\nbrush 0 0 0 1 1 1 unknown 0 0 0 bad\n",
                                "nx3d-brush-map 1\nbrush 0 0 0 nan 1 1\n",
                                "nx3d-brush-map 1\nbrush 0 0 0 1 1 1 extra\n"})
    {
        std::istringstream input(invalid);
        bool rejected = false;
        try { (void)nx3d::readBrushMap(input); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected);
    }
}
