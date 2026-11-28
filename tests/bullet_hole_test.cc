#include <shared/bullet_hole.hh>

#include <stdexcept>

int main()
{
    const nx3d::BulletHole sent{42, {120.25f, 64.5f, -700.0f}, {0.0f, 0.0f, 1.0f}};
    nx3d::BulletHole received;
    if (!nx3d::decodeBulletHole(nx3d::encodeBulletHole(sent), received) ||
        received.id != sent.id || received.position != sent.position ||
        received.normal != sent.normal)
        throw std::runtime_error("Bullet hole event changed during serialization");
    for (const char* invalid : {"0 0 0 0 0 0 1", "1 0 0 0 0 0 0",
                                "1 7000 0 0 0 0 1", "1 0 0 0 0 0 1 extra",
                                "1 0 0 nan 0 0 1"})
        if (nx3d::decodeBulletHole(invalid, received))
            throw std::runtime_error("Invalid bullet hole event was accepted");
    uint64_t cutoff = 0;
    if (!nx3d::decodeBulletHoleReset(nx3d::encodeBulletHoleReset(17), cutoff) || cutoff != 17 ||
        nx3d::decodeBulletHoleReset("0", cutoff) ||
        nx3d::decodeBulletHoleReset("17extra", cutoff))
        throw std::runtime_error("Bullet hole reset cutoff was not validated");
}
