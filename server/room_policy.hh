#ifndef NX3D_SERVER_ROOM_POLICY_HH
#define NX3D_SERVER_ROOM_POLICY_HH

#include <nexilis/room_data.hh>

#include <algorithm>
#include <string_view>

namespace nx3d::server
{
inline bool allowsClientRoomCreation(const nexilis::RoomData& data)
{
    if (data.getContext() != nexilis::RoomData::Context::_3D ||
        data.getMaxSize() < 2 || data.getMaxSize() > 16)
        return false;

    const std::string_view name = data.getName();
    const auto separator = name.find(" | ");
    if (separator == std::string_view::npos)
        return false;
    const auto configuration = name.substr(0, separator);
    const bool supported =
            configuration == "Shipment DM" || configuration == "Shipment Bomb" ||
            configuration == "Shipment DM FF" || configuration == "Shipment Bomb FF" ||
            configuration == "Shipment GunGame" || configuration == "Dust2 DM" ||
            configuration == "Dust2 Bomb" || configuration == "Dust2 GunGame" ||
            configuration == "Dust2 DM FF" || configuration == "Dust2 Bomb FF";
    const auto displayName = name.substr(separator + 3);
    return supported && !displayName.empty() &&
           std::any_of(displayName.begin(), displayName.end(),
                       [](unsigned char c) { return c != ' ' && c != '\t'; }) &&
           std::none_of(displayName.begin(), displayName.end(),
                        [](unsigned char c) { return c < 0x20 || c == 0x7f; });
}
} // namespace nx3d::server

#endif
