#include <shared/brush_mesh.hh>

#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

float area(const std::vector<nx3d::MapVertex>& vertices, bool topOnly = false)
{
    float result = 0;
    for (std::size_t i = 0; i < vertices.size(); i += 3)
    {
        const auto a = vertices[i].position, b = vertices[i+1].position, c = vertices[i+2].position;
        const auto normal = glm::cross(b-a,c-a);
        if (!topOnly || normal.y > 0) result += glm::length(normal) * .5f;
        require(std::isfinite(glm::length(normal)), "Non-finite clipped geometry");
    }
    return result;
}

float totalArea(const nx3d::BrushMesh& mesh)
{
    float result = 0;
    for (const auto& batch : mesh) result += area(batch);
    return result;
}
}

int main()
{
    using namespace nx3d;
    MapBrush cube{glm::vec3(0), glm::vec3(16)};
    auto painted = cube;
    painted.material = "whitewash";
    auto mesh = buildBrushMesh({cube, painted});
    require(mesh[4].empty() && mesh[3].size() == 36, "Duplicate exterior faces were retained");

    auto neighbor = cube;
    neighbor.center.x = 32;
    mesh = buildBrushMesh({cube, neighbor});
    require(std::abs(totalArea(mesh) - 10240) < .1f, "Internal touching faces were retained");

    painted.center.x = 16;
    mesh = buildBrushMesh({cube, painted});
    require(std::abs(totalArea(mesh) - 8192) < .1f, "Partially overlapping faces were not clipped");

    // An inset slab ending at the floor's top reproduces the bridge/terrain overlap.
    MapBrush bridge{glm::vec3(0,14,0), glm::vec3(8,2,8), "whitewash"};
    mesh = buildBrushMesh({cube, bridge});
    require(std::abs(area(mesh[3], true) - 256) < .1f, "Later surface did not own overlapping paint");
    require(std::abs(area(mesh[4], true) - 768) < .1f, "Floor and bridge share a visible face");
    require(std::abs(totalArea(mesh) - 6144) < .1f, "Bridge changed the union's exterior area");

    cube.yaw = 37;
    cube.slopeX = 5;
    painted = cube;
    painted.material = "whitewash";
    mesh = buildBrushMesh({cube, painted});
    require(mesh[4].empty() && mesh[3].size() == 36, "Rotated ramps retain coincident faces");
    std::cout << "Brush surface clipping and ownership passed\n";
}
