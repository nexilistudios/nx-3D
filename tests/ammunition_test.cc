#include <shared/ammunition.hh>

#include <stdexcept>

int main()
{
    int magazine = 3, reserve = 5;
    if (nx3d::ammunition::reload(magazine, reserve, 10) != 5 ||
        magazine != 8 || reserve != 0)
        throw std::runtime_error("Partial reload created extra rounds");
    if (nx3d::ammunition::reload(magazine, reserve, 10) != 0 ||
        magazine != 8 || reserve != 0)
        throw std::runtime_error("Empty reserve allowed another reload");
    reserve = 12;
    if (nx3d::ammunition::reload(magazine, reserve, 10) != 2 ||
        magazine != 10 || reserve != 10)
        throw std::runtime_error("Reload did not stop at magazine capacity");
    const auto item = nx3d::ammunition::encodePickup("ak47", 4, 9);
    int pickedMagazine = -1, pickedReserve = -1;
    if (nx3d::ammunition::pickupWeaponType(item) != "ak47" ||
        !nx3d::ammunition::decodePickup(item, pickedMagazine, pickedReserve) ||
        pickedMagazine != 4 || pickedReserve != 9)
        throw std::runtime_error("Dropped weapon lost its remaining ammunition");
    if (nx3d::ammunition::decodePickup("ak47:-1:9", pickedMagazine, pickedReserve) ||
        nx3d::ammunition::decodePickup("ak47:4:bad", pickedMagazine, pickedReserve))
        throw std::runtime_error("Invalid dropped ammunition was accepted");
}
