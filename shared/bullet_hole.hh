#ifndef NX3D_SHARED_BULLET_HOLE_HH
#define NX3D_SHARED_BULLET_HOLE_HH

#include <glm/glm.hpp>

#include <cmath>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace nx3d
{

inline constexpr std::size_t kMaxBulletHoles = 128;

struct BulletHole
{
    uint64_t id = 0;
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

inline std::string encodeBulletHole(const BulletHole& hole)
{
    std::ostringstream output;
    output << std::setprecision(9) << hole.id << ' '
           << hole.position.x << ' ' << hole.position.y << ' ' << hole.position.z << ' '
           << hole.normal.x << ' ' << hole.normal.y << ' ' << hole.normal.z;
    return output.str();
}

inline bool decodeBulletHole(std::string_view payload, BulletHole& hole)
{
    if (payload.size() > 160)
        return false;
    std::istringstream input{std::string(payload)};
    BulletHole parsed;
    if (!(input >> parsed.id >> parsed.position.x >> parsed.position.y >> parsed.position.z >>
          parsed.normal.x >> parsed.normal.y >> parsed.normal.z))
        return false;
    input >> std::ws;
    if (!input.eof() || parsed.id == 0 ||
        !std::isfinite(parsed.position.x) || !std::isfinite(parsed.position.y) ||
        !std::isfinite(parsed.position.z) || !std::isfinite(parsed.normal.x) ||
        !std::isfinite(parsed.normal.y) || !std::isfinite(parsed.normal.z) ||
        std::abs(parsed.position.x) > 6000.0f ||
        parsed.position.y < -1024.0f || parsed.position.y > 2048.0f ||
        std::abs(parsed.position.z) > 6000.0f ||
        std::abs(glm::length(parsed.normal) - 1.0f) > 0.05f)
        return false;
    hole = parsed;
    return true;
}

inline std::string encodeBulletHoleReset(uint64_t firstValidId)
{
    return std::to_string(firstValidId);
}

inline bool decodeBulletHoleReset(std::string_view payload, uint64_t& firstValidId)
{
    uint64_t parsed = 0;
    const auto [end, error] = std::from_chars(payload.data(),
                                              payload.data() + payload.size(), parsed);
    if (error != std::errc{} || end != payload.data() + payload.size() || parsed == 0)
        return false;
    firstValidId = parsed;
    return true;
}

} // namespace nx3d

#endif
