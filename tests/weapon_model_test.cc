#include <spear/model/obj_loader.hh>
#include <shared/nx_objects.hh>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

size_t validate(const std::string& name, size_t minFaces, size_t minVertices,
                size_t expectedMaterials, float expectedMinZ, float expectedMaxZ)
{
    const auto directory = std::filesystem::path(PROJECT_ROOT) / "assets" / "weapons";
    spear::OBJLoader loader;
    require(loader.load((directory / (name + ".obj")).string(),
                        (directory / (name + ".mtl")).string(), false),
            "Weapon OBJ did not load");

    const auto& faces = loader.getFaces();
    const auto& vertices = loader.getVertices();
    const auto& materials = loader.getMaterials();
    require(faces.size() >= minFaces && vertices.size() >= minVertices,
            "Weapon model is incomplete");
    require(materials.size() == expectedMaterials, "Weapon material palette is incomplete");
    for (const auto& material : materials)
        require(std::filesystem::exists(material.texturePath),
                "Weapon material texture is missing");

    float minZ = 1000.0f, maxZ = -1000.0f;
    for (const auto& vertex : vertices)
    {
        minZ = std::min(minZ, vertex.z);
        maxZ = std::max(maxZ, vertex.z);
    }
    require(minZ < expectedMinZ && maxZ > expectedMaxZ, "Weapon is not aligned along +Z");

    for (const auto& face : faces)
    {
        require(face.vertexIndices.size() == 3, "Weapon face is not a triangle");
        for (int index : face.vertexIndices)
            require(index >= 0 && static_cast<size_t>(index) < vertices.size(),
                    "Weapon vertex index out of range");
    }
    return faces.size();
}

void validateReloadParts(const std::string& name)
{
    const auto directory = std::filesystem::path(PROJECT_ROOT) / "assets" / "weapons";
    const auto material = (directory / (name + ".mtl")).string();
    spear::OBJLoader complete, body, magazine;
    require(complete.load((directory / (name + ".obj")).string(), material, false),
            "Complete gun OBJ did not load");
    require(body.load((directory / (name + "_body.obj")).string(), material, false),
            "Gun body OBJ did not load");
    require(magazine.load((directory / (name + "_magazine.obj")).string(), material, false),
            "Magazine OBJ did not load");
    require(!body.getFaces().empty() && !magazine.getFaces().empty() &&
                    body.getFaces().size() + magazine.getFaces().size() ==
                            complete.getFaces().size(),
            "Gun body and magazine do not reconstruct the complete gun");
}
} // namespace

int main()
{
    try
    {
        require(validate("ak47", 350, 700, 4, -1.0f, 1.8f) !=
                        validate("m16", 350, 700, 4, -1.0f, 1.8f),
                "AK-47 and M16 should have distinct geometry");
        for (const auto& pistol : {"glock", "usp", "deagle"})
            validate(pistol, 60, 100, 3, -0.35f, 0.7f);
        validate("mp5", 120, 200, 3, -1.0f, 1.5f);
        for (const auto& gun : {"ak47", "m16", "glock", "usp", "deagle", "mp5"})
            validateReloadParts(gun);
        validate("knife", 20, 16, 2, -0.8f, 0.7f);
        validate("grenade", 20, 16, 2, -0.15f, 0.15f);
        for (const auto& object : nx3d::kNXObjects)
        {
            spear::OBJLoader loader;
            const auto assets = std::filesystem::path(PROJECT_ROOT) / "assets";
            require(loader.load((assets / object.visual.worldModel).string(),
                                (assets / object.visual.worldMaterial).string(), false) &&
                            !loader.getFaces().empty(),
                    "NXObject world mesh did not load");
            for (const auto& material : loader.getMaterials())
                require(std::filesystem::exists(material.texturePath),
                        "NXObject material texture is missing");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
