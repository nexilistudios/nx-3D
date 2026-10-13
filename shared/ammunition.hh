#ifndef NX3D_SHARED_AMMUNITION_HH
#define NX3D_SHARED_AMMUNITION_HH

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>

namespace nx3d::ammunition
{

inline int reload(int& magazine, int& reserve, int capacity)
{
    const int loaded = std::min(std::max(0, capacity - magazine), std::max(0, reserve));
    magazine += loaded;
    reserve -= loaded;
    return loaded;
}

inline std::string_view pickupWeaponType(std::string_view itemType)
{
    return itemType.substr(0, itemType.find(':'));
}

inline std::string encodePickup(std::string_view weaponType, int magazine, int reserve)
{
    return std::string(weaponType) + ":" + std::to_string(magazine) + ":" +
           std::to_string(reserve);
}

inline bool decodePickup(std::string_view itemType, int& magazine, int& reserve)
{
    const auto first = itemType.find(':');
    if (first == std::string_view::npos) return false;
    const auto second = itemType.find(':', first + 1);
    if (second == std::string_view::npos) return false;
    int nextMagazine = 0, nextReserve = 0;
    const auto magText = itemType.substr(first + 1, second - first - 1);
    const auto reserveText = itemType.substr(second + 1);
    const auto magResult = std::from_chars(magText.data(), magText.data() + magText.size(), nextMagazine);
    const auto reserveResult = std::from_chars(reserveText.data(), reserveText.data() + reserveText.size(), nextReserve);
    if (magResult.ec != std::errc{} || magResult.ptr != magText.data() + magText.size() ||
        reserveResult.ec != std::errc{} || reserveResult.ptr != reserveText.data() + reserveText.size() ||
        nextMagazine < 0 || nextReserve < 0)
        return false;
    magazine = nextMagazine;
    reserve = nextReserve;
    return true;
}

} // namespace nx3d::ammunition

#endif
