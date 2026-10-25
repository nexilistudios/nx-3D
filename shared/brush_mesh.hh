#ifndef NX3D_BRUSH_MESH_HH
#define NX3D_BRUSH_MESH_HH

#include <shared/brush_map.hh>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace nx3d
{
using BrushMesh = std::array<std::vector<MapVertex>, kMapMaterials.size()>;

namespace brush_mesh_detail
{
constexpr float kEpsilon = .001f;
constexpr float kBucketSize = 256.0f;
using Polygon = std::vector<MapVertex>;
struct Plane { glm::vec3 normal; float distance; };
struct Solid
{
    glm::vec3 minimum{INFINITY}, maximum{-INFINITY};
    std::array<Plane, 6> planes;
    std::array<MapVertex, 36> vertices;
};

inline bool overlaps(const glm::vec3& amin, const glm::vec3& amax,
                     const glm::vec3& bmin, const glm::vec3& bmax)
{
    for (int axis = 0; axis < 3; ++axis)
        if (amax[axis] < bmin[axis] - kEpsilon || amin[axis] > bmax[axis] + kEpsilon)
            return false;
    return true;
}

inline bool hasArea(const Polygon& polygon)
{
    if (polygon.size() < 3) return false;
    glm::vec3 area(0);
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
        area += glm::cross(polygon[i].position - polygon[0].position,
                           polygon[i + 1].position - polygon[0].position);
    return glm::dot(area, area) > kEpsilon * kEpsilon;
}

inline Polygon clip(const Polygon& polygon, const Plane& plane, bool inside)
{
    Polygon result;
    if (polygon.empty()) return result;
    auto previous = polygon.back();
    float previousDistance = glm::dot(plane.normal, previous.position) - plane.distance;
    for (const auto& current : polygon)
    {
        const float distance = glm::dot(plane.normal, current.position) - plane.distance;
        const bool previousKept = inside ? previousDistance <= kEpsilon : previousDistance > kEpsilon;
        const bool currentKept = inside ? distance <= kEpsilon : distance > kEpsilon;
        if (previousKept != currentKept)
        {
            const float t = std::clamp(previousDistance / (previousDistance - distance), 0.0f, 1.0f);
            result.push_back({glm::mix(previous.position, current.position, t),
                              glm::mix(previous.uv, current.uv, t)});
        }
        if (currentKept) result.push_back(current);
        previous = current;
        previousDistance = distance;
    }
    return result;
}

// Partition the portion of a face outside a convex solid into convex polygons.
inline void subtract(const Polygon& face, const Solid& solid, std::vector<Polygon>& result)
{
    Polygon remaining = face;
    for (const auto& plane : solid.planes)
    {
        auto outside = clip(remaining, plane, false);
        if (hasArea(outside)) result.push_back(std::move(outside));
        remaining = clip(remaining, plane, true);
        if (!hasArea(remaining)) return;
    }
}

inline std::array<int, 3> bucket(const glm::vec3& point)
{
    return {static_cast<int>(std::floor(point.x / kBucketSize)),
            static_cast<int>(std::floor(point.y / kBucketSize)),
            static_cast<int>(std::floor(point.z / kBucketSize))};
}
} // namespace brush_mesh_detail

// Compile the union of editable solids. Adjacent cubes share no internal faces;
// overlapping exterior faces have one deterministic owner (the later brush).
// Both collision and rendering consume this mesh, including deformed/rotated cubes.
inline BrushMesh buildBrushMesh(const std::vector<MapBrush>& brushes)
{
    using namespace brush_mesh_detail;
    std::vector<Solid> solids;
    std::map<std::array<int, 3>, std::vector<std::size_t>> buckets;
    for (const auto& brush : brushes)
    {
        Solid solid;
        solid.vertices = brushVertices(brush);
        for (const auto& vertex : solid.vertices)
        {
            solid.minimum = glm::min(solid.minimum, vertex.position);
            solid.maximum = glm::max(solid.maximum, vertex.position);
        }
        for (std::size_t face = 0; face < 6; ++face)
        {
            const auto a = solid.vertices[face * 6].position;
            const auto b = solid.vertices[face * 6 + 1].position;
            const auto c = solid.vertices[face * 6 + 2].position;
            const auto normal = glm::normalize(glm::cross(b - a, c - a));
            solid.planes[face] = {normal, glm::dot(normal, a)};
        }
        const auto first = bucket(solid.minimum - glm::vec3(kEpsilon));
        const auto last = bucket(solid.maximum + glm::vec3(kEpsilon));
        for (int x = first[0]; x <= last[0]; ++x)
            for (int y = first[1]; y <= last[1]; ++y)
                for (int z = first[2]; z <= last[2]; ++z)
                    buckets[{x,y,z}].push_back(solids.size());
        solids.push_back(std::move(solid));
    }
    BrushMesh mesh;
    for (std::size_t source = 0; source < solids.size(); ++source)
    {
        const auto& solid = solids[source];
        const auto material = std::find(kMapMaterials.begin(), kMapMaterials.end(), brushes[source].material);
        if (material == kMapMaterials.end()) throw std::runtime_error("Unknown brush mesh material");
        auto& output = mesh[std::distance(kMapMaterials.begin(), material)];
        for (std::size_t face = 0; face < 6; ++face)
        {
            Polygon polygon{solid.vertices[face*6], solid.vertices[face*6+1],
                            solid.vertices[face*6+2], solid.vertices[face*6+5]};
            glm::vec3 minimum(INFINITY), maximum(-INFINITY);
            for (const auto& vertex : polygon)
            {
                minimum = glm::min(minimum, vertex.position);
                maximum = glm::max(maximum, vertex.position);
            }
            const auto first = bucket(minimum - glm::vec3(kEpsilon));
            const auto last = bucket(maximum + glm::vec3(kEpsilon));
            std::set<std::size_t> candidates;
            for (int x = first[0]; x <= last[0]; ++x)
                for (int y = first[1]; y <= last[1]; ++y)
                    for (int z = first[2]; z <= last[2]; ++z)
                    {
                        const auto found = buckets.find({x,y,z});
                        if (found != buckets.end()) candidates.insert(found->second.begin(), found->second.end());
                    }
            std::vector<Polygon> fragments{std::move(polygon)};
            for (const auto target : candidates)
            {
                if (target == source || !overlaps(minimum, maximum, solids[target].minimum, solids[target].maximum))
                    continue;
                // Coincident outward faces must not erase one another.
                const auto& plane = solid.planes[face];
                bool earlierCoplanar = false;
                if (target < source)
                    for (const auto& other : solids[target].planes)
                        if (glm::dot(plane.normal, other.normal) > .999999f &&
                            std::abs(plane.distance - other.distance) <= kEpsilon)
                            earlierCoplanar = true;
                if (earlierCoplanar) continue;
                std::vector<Polygon> visible;
                for (const auto& fragment : fragments) subtract(fragment, solids[target], visible);
                fragments = std::move(visible);
                if (fragments.empty()) break;
            }
            for (const auto& fragment : fragments)
                for (std::size_t i = 1; i + 1 < fragment.size(); ++i)
                {
                    const auto area = glm::cross(fragment[i].position - fragment[0].position,
                                                  fragment[i+1].position - fragment[0].position);
                    if (glm::dot(area, area) <= kEpsilon*kEpsilon) continue;
                    output.insert(output.end(), {fragment[0], fragment[i], fragment[i+1]});
                }
        }
    }
    return mesh;
}
} // namespace nx3d
#endif
