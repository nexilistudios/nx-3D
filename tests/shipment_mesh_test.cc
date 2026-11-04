#include <shared/brush_mesh.hh>
#include <shared/gamemode.hh>

#include <fstream>
#include <stdexcept>

int main()
{
    if (std::string_view(nx3d::mapPathForRoom(nx3d::kShipmentRoomName)) !=
                nx3d::kShipmentMapPath ||
        std::string_view(nx3d::mapPathForRoom("Shipment Bomb")) !=
                nx3d::kShipmentMapPath ||
        std::string_view(nx3d::mapPathForRoom("Shipment DM | My Room")) !=
                nx3d::kShipmentMapPath ||
        std::string_view(nx3d::mapPathForRoom(nx3d::kDust2RoomName)) !=
                nx3d::kGeneratedMapPath)
        throw std::runtime_error("Room names select the wrong maps");
    if (nx3d::gameModeFromName("Shipment DM | Bomb fans") != nx3d::GameMode::deathmatch ||
        nx3d::gameModeFromName("Dust2 Bomb | Deathmatch fans") != nx3d::GameMode::bomb ||
        nx3d::roomDisplayName("Shipment DM | My Room") != "My Room")
        throw std::runtime_error("Custom room names changed room settings or were not displayed");
    std::ifstream map(std::string(PROJECT_ROOT) + "/assets/maps/shipment/map.brush");
    if (!map)
        throw std::runtime_error("Cannot open Shipment map");
    const auto brushes = nx3d::readBrushMap(map);
    const auto mesh = nx3d::buildBrushMesh(brushes);
    std::size_t triangles = 0;
    for (const auto& material : mesh)
        triangles += material.size() / 3;
    if (brushes.size() < 300 || triangles < 1000)
        throw std::runtime_error("Shipment did not produce usable collision geometry");
}
