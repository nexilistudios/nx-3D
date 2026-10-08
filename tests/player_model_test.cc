#include <spear/model/obj_loader.hh>

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

size_t validate(const std::string& name)
{
    const std::filesystem::path directory =
            std::filesystem::path(PROJECT_ROOT) / "assets" / "players";
    spear::OBJLoader loader;
    require(loader.load((directory / (name + ".obj")).string(),
                        (directory / (name + ".mtl")).string(), false),
            "Player OBJ did not load");
    const auto& faces = loader.getFaces();
    const auto& vertices = loader.getVertices();
    const auto& uvs = loader.getUvs();
    const auto& materials = loader.getMaterials();
    require(faces.size() > 500 && !vertices.empty(), "Player mesh is incomplete");
    require(materials.size() == 1 &&
                    std::filesystem::exists(materials.front().texturePath),
            "Player color atlas is missing");

    float minY = 1000.0f, maxY = -1000.0f;
    for (const auto& vertex : vertices)
    {
        minY = std::min(minY, vertex.y);
        maxY = std::max(maxY, vertex.y);
    }
    require(minY <= -64.0f && maxY > 9.0f,
            "Player mesh is not aligned to eye height");
    for (const auto& face : faces)
    {
        require(face.vertexIndices.size() == 3 && face.textureCoordIndices.size() == 3,
                "Player face lacks UV coordinates");
        for (int index : face.vertexIndices)
            require(index >= 0 && static_cast<size_t>(index) < vertices.size(),
                    "Player vertex index out of range");
        for (int index : face.textureCoordIndices)
            require(index >= 0 && static_cast<size_t>(index) < uvs.size(),
                    "Player UV index out of range");
    }
    return faces.size();
}
} // namespace

int main()
{
    try
    {
        require(validate("counter_terrorist") != validate("terrorist"),
                "T and CT meshes should have distinct equipment");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
