#ifndef NX3D_PROJECT_ROOT
#define NX3D_PROJECT_ROOT

#include <filesystem>
#include <string>

namespace nx3d
{

// Defined in CMake as the repository root.
inline const std::string& projectRoot()
{
    static const std::string root = PROJECT_ROOT;
    return root;
}

// Resolve a repo-root-relative asset path to a filesystem path.
inline std::string projectAssetPath(const std::string& relative = "")
{
    return (std::filesystem::path(projectRoot()) / "assets" / relative).string();
}

} // namespace nx3d

#endif