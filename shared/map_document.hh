#ifndef NX3D_MAP_DOCUMENT_HH
#define NX3D_MAP_DOCUMENT_HH

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <shared/brush_map.hh>

namespace nx3d
{
struct MapProp
{
    std::string model = "props/example_crate.obj";
    glm::vec3 center{0};
    glm::vec3 halfSize{32};
    float yaw = 0;
    bool solid = true;
    bool operator==(const MapProp&) const = default;
};
struct MapDocument
{
    std::vector<MapBrush> brushes;
    std::vector<MapProp> props;
    bool operator==(const MapDocument&) const = default;
};

inline void validateMapProp(const MapProp& prop)
{
    const auto path = std::filesystem::path(prop.model);
    if (path.empty() || path.is_absolute() || path.extension() != ".obj" ||
        prop.model.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("Models must be OBJ paths relative to assets/");
    for (const auto& part : path)
        if (part == "..")
            throw std::runtime_error("Model paths cannot leave assets/");
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(prop.center[axis]) || std::abs(prop.center[axis]) > 100000 ||
            !std::isfinite(prop.halfSize[axis]) || prop.halfSize[axis] <= 0 || prop.halfSize[axis] > 100000)
            throw std::runtime_error("Invalid model position or size");
    if (!std::isfinite(prop.yaw) || std::abs(prop.yaw) > 100000)
        throw std::runtime_error("Invalid model rotation");
}

inline MapBrush propBounds(const MapProp& prop)
{
    return {prop.center, prop.halfSize, "stone", 0, 0, prop.yaw, "prop/collision"};
}

// Model records are versioned comments, preserving the existing brush format
// and compatibility with geometry tools that do not render models.
inline MapDocument readMapDocument(std::istream& input)
{
    const std::string contents{std::istreambuf_iterator<char>(input), {}};
    std::istringstream geometry(contents), records(contents);
    MapDocument result;
    result.brushes = readBrushMap(geometry);
    std::string line;
    while (std::getline(records, line))
    {
        std::istringstream record(line);
        std::string comment, version, extra;
        record >> comment >> version;
        if (comment != "#" || version != "nx3d-model-v1")
            continue;
        MapProp prop;
        int solid = 1;
        if (!(record >> std::quoted(prop.model) >> prop.center.x >> prop.center.y >> prop.center.z >> prop.halfSize.x >> prop.halfSize.y >> prop.halfSize.z >> prop.yaw >> solid) ||
            (solid != 0 && solid != 1) || (record >> extra))
            throw std::runtime_error("Invalid model record");
        prop.solid = solid != 0;
        validateMapProp(prop);
        result.props.push_back(prop);
        if (result.props.size() > 4096)
            throw std::runtime_error("Map exceeds 4096 models");
    }
    return result;
}

inline void writeMapDocument(std::ostream& out, const MapDocument& document)
{
    if (document.brushes.size() > 32768 || document.props.size() > 4096)
        throw std::runtime_error("Map object limit exceeded");
    out << std::setprecision(std::numeric_limits<float>::max_digits10)
        << "nx3d-brush-map 2\n# Edited with the nx-3D map editor.\n";
    for (const auto& b : document.brushes)
        out << "brush " << b.center.x << ' ' << b.center.y << ' ' << b.center.z << ' '
            << b.halfSize.x << ' ' << b.halfSize.y << ' ' << b.halfSize.z << ' '
            << b.material << ' ' << b.slopeX << ' ' << b.slopeZ << ' ' << b.yaw << ' ' << b.group << '\n';
    for (const auto& p : document.props)
    {
        validateMapProp(p);
        out << "# nx3d-model-v1 " << std::quoted(p.model) << ' '
            << p.center.x << ' ' << p.center.y << ' ' << p.center.z << ' '
            << p.halfSize.x << ' ' << p.halfSize.y << ' ' << p.halfSize.z << ' '
            << p.yaw << ' ' << p.solid << '\n';
    }
}

inline void saveMapDocument(const std::filesystem::path& path, const MapDocument& document)
{
    std::ostringstream serialized;
    writeMapDocument(serialized, document);
    std::istringstream validation(serialized.str());
    (void)readMapDocument(validation);
    // Stage beside the destination, then replace it atomically. A failed write
    // leaves the original file intact. Keep a backup for recovery after editing.
    const auto temporary = std::filesystem::path(path.string() + ".saving");
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    try
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << serialized.str();
        out.close();
        if (!out)
            throw std::runtime_error("Could not write map: " + path.string());
        if (std::filesystem::exists(path))
            std::filesystem::copy_file(path, path.string() + ".bak", std::filesystem::copy_options::overwrite_existing);
        // Windows does not replace an existing destination with rename().
#ifdef _WIN32
        if (std::filesystem::exists(path))
            std::filesystem::remove(path);
#endif
        std::filesystem::rename(temporary, path);
    }
    catch (...)
    {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        throw;
    }
}
} // namespace nx3d
#endif
