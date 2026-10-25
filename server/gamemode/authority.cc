#include <server/gamemode/authority.hh>

#include <shared/economy.hh>
#include <shared/brush_mesh.hh>
#include <shared/player_hit.hh>
#include <shared/player_rules.hh>

#include <nexilis/command_type.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/gameplay_limits.hh>

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace nx3d::server::gamemode
{
namespace
{
constexpr float kEyeHeight = nx3d::kStandingEyeHeight;
constexpr float kStepHeight = 24.0f;
constexpr float kRadius = 7.68f;
constexpr float kGravity = -800.0f;
constexpr float kJumpSpeed = 252.0f;

btVector3 asBullet(const glm::vec3& v)
{
    return {v.x, v.y, v.z};
}

glm::vec3 asGlm(const nexilis::Vector3f& v)
{
    return {v.x, v.y, v.z};
}

struct ContactCheck : btCollisionWorld::ContactResultCallback
{
    bool blocked = false;

    btScalar addSingleResult(btManifoldPoint&, const btCollisionObjectWrapper*, int, int,
                             const btCollisionObjectWrapper*, int, int) override
    {
        blocked = true;
        return 0.0f;
    }
};

class MapPhysics
{
public:
    MapPhysics()
        : m_dispatcher(&m_configuration),
          m_world(&m_dispatcher, &m_broadphase, &m_solver, &m_configuration)
    {
        const auto relative = std::filesystem::path("assets") / nx3d::kGeneratedMapPath;
        const char* assetRoot = std::getenv("NX3D_ASSET_ROOT");
        const auto configured = assetRoot ? std::filesystem::path(assetRoot) / relative
                                          : std::filesystem::path(PROJECT_ROOT) / relative;
        const auto path = std::filesystem::exists(configured) ? configured : relative;
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot load generated server map: " + path.string());
        const auto mesh = nx3d::buildBrushMesh(nx3d::readBrushMap(file));
        for (const auto& vertices : mesh)
        {
            for (std::size_t i = 0; i < vertices.size(); i += 3)
            {
                m_mesh.addTriangle(asBullet(vertices[i].position),
                                   asBullet(vertices[i + 1].position),
                                   asBullet(vertices[i + 2].position));
            }
        }
        if (m_mesh.getNumTriangles() == 0)
            throw std::runtime_error("Empty generated server map");
        m_shape = std::make_unique<btBvhTriangleMeshShape>(&m_mesh, true);
        btTransform transform;
        transform.setIdentity();
        m_motion = std::make_unique<btDefaultMotionState>(transform);
        btRigidBody::btRigidBodyConstructionInfo info(0.0f, m_motion.get(), m_shape.get());
        m_body = std::make_unique<btRigidBody>(info);
        m_world.addRigidBody(m_body.get());
    }

    ~MapPhysics()
    {
        m_world.removeRigidBody(m_body.get());
    }

    bool blocks(const glm::vec3& from, const glm::vec3& to)
    {
        if (glm::distance(from, to) < 0.001f)
            return false;
        std::lock_guard lock(m_mutex);
        btCollisionWorld::ClosestRayResultCallback hit(asBullet(from), asBullet(to));
        m_world.rayTest(asBullet(from), asBullet(to), hit);
        return hit.hasHit() && hit.m_closestHitFraction < 1.0f;
    }

    void advance(glm::vec3& position, const glm::vec3& direction, bool jump,
                 bool crouchRequested, bool silentWalk, bool& crouched,
                 bool& jumpHeld, bool& grounded, float& verticalVelocity, float dt)
    {
        std::lock_guard lock(m_mutex);
        if (crouchRequested && !crouched)
        {
            position.y -= kEyeHeight - nx3d::kCrouchingEyeHeight;
            crouched = true;
        }
        else if (!crouchRequested && crouched)
        {
            const float totalHeight = kEyeHeight * 0.9f;
            const float feetY = position.y - nx3d::kCrouchingEyeHeight;
            btCapsuleShape capsule(kRadius, totalHeight - 2.0f * kRadius);
            btCollisionObject standing;
            standing.setCollisionShape(&capsule);
            btTransform transform;
            transform.setIdentity();
            transform.setOrigin(btVector3(position.x,
                                          feetY + kEyeHeight * 0.15f + totalHeight * 0.5f,
                                          position.z));
            standing.setWorldTransform(transform);
            ContactCheck contacts;
            m_world.contactTest(&standing, contacts);
            if (!contacts.blocked)
            {
                position.y += kEyeHeight - nx3d::kCrouchingEyeHeight;
                crouched = false;
            }
        }
        const float eyeHeight = crouched ? nx3d::kCrouchingEyeHeight : kEyeHeight;
        const float previousHeadY = position.y;
        const bool wasGrounded = grounded;
        if (glm::length(glm::vec2(direction.x, direction.z)) > 0.0f)
        {
            const float speed = crouched ? nx3d::kCrouchSpeed : silentWalk ? nx3d::kSilentWalkSpeed
                                                                           : nx3d::kWalkSpeed;
            glm::vec3 target = position + direction * speed * dt;
            const float capsuleY = position.y - eyeHeight + eyeHeight * 0.15f +
                                   eyeHeight * 0.9f * 0.5f;
            btCapsuleShape capsule(kRadius, eyeHeight * 0.9f - 2.0f * kRadius);
            btTransform from, to;
            from.setIdentity();
            to.setIdentity();
            from.setOrigin(btVector3(position.x, capsuleY, position.z));
            to.setOrigin(btVector3(target.x, capsuleY, target.z));
            btCollisionWorld::ClosestConvexResultCallback hit(from.getOrigin(), to.getOrigin());
            m_world.convexSweepTest(&capsule, from, to, hit);
            if (hit.hasHit())
            {
                bool stepped = false;
                if (grounded)
                {
                    btTransform raisedFrom = from, raisedTo = to;
                    raisedFrom.getOrigin() += btVector3(0, kStepHeight, 0);
                    raisedTo.getOrigin() += btVector3(0, kStepHeight, 0);
                    btCollisionWorld::ClosestConvexResultCallback lift(from.getOrigin(), raisedFrom.getOrigin());
                    m_world.convexSweepTest(&capsule, from, raisedFrom, lift);
                    if (!lift.hasHit())
                    {
                        btCollisionWorld::ClosestConvexResultCallback raised(raisedFrom.getOrigin(),
                                                                             raisedTo.getOrigin());
                        m_world.convexSweepTest(&capsule, raisedFrom, raisedTo, raised);
                        if (!raised.hasHit())
                        {
                            auto ground = groundY(target.x, target.z, position.y + kStepHeight,
                                                  position.y - eyeHeight - 1.0f);
                            if (ground && *ground - (position.y - eyeHeight) > 0.1f &&
                                *ground - (position.y - eyeHeight) <= kStepHeight + 0.1f)
                            {
                                target.y = *ground + eyeHeight;
                                verticalVelocity = 0.0f;
                                stepped = true;
                            }
                        }
                    }
                }
                if (!stepped)
                {
                    const float fraction = std::max(0.0f, hit.m_closestHitFraction - 0.01f);
                    const glm::vec3 remaining = target - position;
                    target = position + remaining * fraction;
                    btVector3 normal = hit.m_hitNormalWorld;
                    normal.setY(0.0f);
                    if (normal.length2() > 0.01f)
                    {
                        normal.normalize();
                        glm::vec3 slide = remaining * (1.0f - fraction);
                        const float into = slide.x * normal.x() + slide.z * normal.z();
                        if (into < 0.0f)
                        {
                            slide.x -= into * normal.x();
                            slide.z -= into * normal.z();
                        }
                        btTransform slideFrom = from, slideTo = from;
                        slideFrom.setOrigin(btVector3(target.x, capsuleY, target.z));
                        slideTo.setOrigin(slideFrom.getOrigin() + btVector3(slide.x, 0, slide.z));
                        btCollisionWorld::ClosestConvexResultCallback slideHit(slideFrom.getOrigin(),
                                                                               slideTo.getOrigin());
                        m_world.convexSweepTest(&capsule, slideFrom, slideTo, slideHit);
                        target += slide * (slideHit.hasHit()
                                                   ? std::max(0.0f, slideHit.m_closestHitFraction - 0.01f)
                                                   : 1.0f);
                    }
                }
            }
            position = target;
        }

        if (jump && !jumpHeld && grounded && !crouched)
        {
            verticalVelocity = kJumpSpeed;
            grounded = false;
        }
        jumpHeld = jump;
        if (!grounded)
            verticalVelocity += kGravity * dt;
        constexpr std::array<std::array<float, 2>, 5> probes{{{{0, 0}}, {{kRadius * 0.9f, 0}}, {{-kRadius * 0.9f, 0}}, {{0, kRadius * 0.9f}}, {{0, -kRadius * 0.9f}}}};
        float nextY = position.y + verticalVelocity * dt;
        if (nextY > position.y)
        {
            for (const auto& probe : probes)
            {
                const btVector3 from(position.x + probe[0], position.y + 3.2f,
                                     position.z + probe[1]);
                const btVector3 to(from.x(), nextY + 3.2f, from.z());
                btCollisionWorld::ClosestRayResultCallback hit(from, to);
                m_world.rayTest(from, to, hit);
                if (hit.hasHit() && hit.m_hitNormalWorld.y() < -0.5f)
                {
                    nextY = std::min(nextY, hit.m_hitPointWorld.y() - 3.3f);
                    verticalVelocity = 0.0f;
                }
            }
        }
        position.y = nextY;

        grounded = false;
        std::optional<float> ground;
        for (const auto& probe : probes)
        {
            auto hit = groundY(position.x + probe[0], position.z + probe[1],
                               std::max(previousHeadY, position.y),
                               position.y - eyeHeight - kStepHeight);
            const float allowedRise = wasGrounded ? kStepHeight + 0.1f : 0.1f;
            if (hit && *hit <= previousHeadY - eyeHeight + allowedRise &&
                (!ground || *hit > *ground))
                ground = hit;
        }
        const float feetY = position.y - eyeHeight;
        if (ground && verticalVelocity <= 0.0f &&
            (feetY <= *ground + 0.1f || (wasGrounded && feetY - *ground <= kStepHeight)))
        {
            position.y = *ground + eyeHeight;
            verticalVelocity = 0.0f;
            grounded = true;
        }
    }

private:
    std::optional<float> groundY(float x, float z, float fromY, float toY)
    {
        btCollisionWorld::ClosestRayResultCallback hit(btVector3(x, fromY, z),
                                                       btVector3(x, toY, z));
        m_world.rayTest(btVector3(x, fromY, z), btVector3(x, toY, z), hit);
        if (!hit.hasHit() || hit.m_hitNormalWorld.y() < 0.65f)
            return std::nullopt;
        return hit.m_hitPointWorld.y();
    }

    btDefaultCollisionConfiguration m_configuration;
    btCollisionDispatcher m_dispatcher;
    btDbvtBroadphase m_broadphase;
    btSequentialImpulseConstraintSolver m_solver;
    btDiscreteDynamicsWorld m_world;
    btTriangleMesh m_mesh;
    std::unique_ptr<btBvhTriangleMeshShape> m_shape;
    std::unique_ptr<btDefaultMotionState> m_motion;
    std::unique_ptr<btRigidBody> m_body;
    std::mutex m_mutex;
};

MapPhysics& mapPhysics()
{
    static MapPhysics physics;
    return physics;
}

void publishPosition(nexilis::server::Room& room, nexilis::server::User& user)
{
    const auto pos = user.getObject3D().getPosition();
    nexilis::nx_data data{
            static_cast<uint8_t>(nexilis::CommandType::room),
            static_cast<uint8_t>(nexilis::RoomCommandType::Root::player_3D),
            static_cast<uint8_t>(nexilis::RoomCommandType::PlayerType::position)};
    room.broadcastToAll(nexilis::server::Command::createRoomCommand(
            room.getId(), user, data,
            {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}, {"yaw", user.getObject3D().getYaw()}}, 0));
}

void publishStance(nexilis::server::Room& room, nexilis::server::User& user, bool crouched)
{
    const float height = crouched ? nx3d::kCrouchingEyeHeight : nx3d::kStandingEyeHeight;
    user.getObject3D().setDimensions({36.0f, height, 18.0f});
    room.broadcastToAll(nexilis::server::Command::clientMessageData(
            nexilis::CommandType::room, "player_3D", 0,
            {{"action", "dimensions"}, {"room_id", room.getId()}, {"client_id", user.getId()}, {"x", 36.0f}, {"y", height}, {"z", 18.0f}}));
}

void publishAnimation(nexilis::server::Room& room, nexilis::server::User& user,
                      const std::string& state, float duration = 0.0f,
                      const std::string& weapon = {})
{
    room.broadcastToAll(nexilis::server::Command::clientMessageData(
            nexilis::CommandType::room, "player_3D", 0,
            {{"action", "animation"}, {"room_id", room.getId()},
             {"client_id", user.getId()}, {"state", state.c_str()},
             {"weapon", weapon.c_str()}, {"duration", duration}}));
}

struct WeaponRule
{
    float damage;
    float roundsPerMinute;
    int magazine;
    int reserveMagazines;
    float reloadSeconds;
    float range;
};

std::optional<WeaponRule> weaponRule(const std::string& type)
{
    const auto* object = nx3d::findNXObject(type);
    if (!object || (object->kind != nx3d::NXObjectKind::firearm &&
                    object->kind != nx3d::NXObjectKind::melee))
        return std::nullopt;
    const auto& combat = object->combat;
    return WeaponRule{combat.damage, combat.roundsPerMinute,
                      combat.magazineSize, combat.reserveMagazines,
                      combat.reloadSeconds,
                      object->kind == nx3d::NXObjectKind::melee ? 100.0f : 5000.0f};
}
} // namespace

Authority::Authority()
{
    (void)mapPhysics();
}

void Authority::spawn(nexilis::server::Room& room, uint64_t playerId, bool preserveEquipment)
{
    auto* user = nexilis::server::ClientStorage::getClientById(playerId);
    if (!user)
        return;
    const auto team = room.getPlayerTeam(playerId);
    if (team != "Terrorist" && team != "Counter Terrorist")
        return;
    const auto& spawns = team == "Terrorist" ? nx3d::economy::kTSpawns
                                             : nx3d::economy::kCtSpawns;
    nx3d::economy::SpawnPoint point;
    std::string equipped;
    {
        std::lock_guard lock(m_mutex);
        point = spawns[m_spawnCounter++ % spawns.size()];
        auto& state = m_players[playerId];
        const auto primary = preserveEquipment ? state.primary : WeaponState{};
        const auto secondary = preserveEquipment ? state.secondary : WeaponState{};
        const auto previousWeapon = preserveEquipment ? state.activeWeapon : std::string{};
        state = PlayerState{};
        state.lastInput = std::chrono::steady_clock::now();
        state.primary = primary;
        state.secondary = secondary;
        state.primary.reloadEnd = {};
        state.secondary.reloadEnd = {};
        state.activeWeapon = previousWeapon.empty() || previousWeapon == "bomb"
                                     ? std::string(nx3d::economy::defaultWeapon(team))
                                     : previousWeapon;
        equipped = state.activeWeapon;
    }
    user->getObject3D().setPosition({point.x, point.y, point.z});
    publishStance(room, *user, false);
    publishPosition(room, *user);
    publishAnimation(room, *user, "idle", 0.0f, equipped);
}

bool Authority::move(nexilis::server::Room& room, uint64_t playerId,
                     const nexilis::Vector3f& input, float yaw)
{
    auto* user = nexilis::server::ClientStorage::getClientById(playerId);
    if (!user || user->getRoomId() != room.getId())
        return false;
    glm::vec3 direction(input.x, 0, input.z);
    const float length = glm::length(direction);
    if (!std::isfinite(length))
        return false;
    if (length > 1.0f)
        direction /= length;
    if (!std::isfinite(yaw) || !std::isfinite(input.y))
        return false;
    // Packets can arrive before the team/spawn command or after a death. They
    // are valid input, but must not affect a player who cannot move yet.
    if (room.getPlayerTeam(playerId).empty() || room.getPlayerHealth(playerId) <= 0.0f)
        return true;
    std::lock_guard lock(m_mutex);
    auto it = m_players.find(playerId);
    if (it == m_players.end())
        return true;
    // Older clients used Y as a jump boolean. Keep accepting their packets,
    // including the oversized test input that exercises speed clamping.
    const uint8_t flags = input.y >= 0.0f && input.y <= 7.0f &&
                                          std::floor(input.y) == input.y
                                  ? static_cast<uint8_t>(input.y)
                                  : (input.y > 0.5f ? nx3d::kMoveJump : 0);
    it->second.input = {direction.x, static_cast<float>(flags), direction.z};
    it->second.yaw = std::remainder(yaw, 6.2831853f);
    it->second.lastInput = std::chrono::steady_clock::now();
    return true;
}

void Authority::update(nexilis::server::Room& room, float dt)
{
    dt = std::clamp(dt, 0.0f, 0.05f);
    const auto now = std::chrono::steady_clock::now();
    for (auto playerId : room.getClients())
    {
        auto* user = nexilis::server::ClientStorage::getClientById(playerId);
        if (!user || user->getRoomId() != room.getId() ||
            room.getPlayerTeam(playerId).empty() || room.getPlayerHealth(playerId) <= 0.0f)
            continue;
        bool stanceChanged = false;
        bool crouched = false;
        bool animationChanged = false;
        std::string animation;
        std::string activeWeapon;
        {
            std::lock_guard lock(m_mutex);
            auto it = m_players.find(playerId);
            if (it == m_players.end())
                continue;
            auto& state = it->second;
            const bool fresh = now - state.lastInput < std::chrono::milliseconds(250);
            const glm::vec3 direction = fresh ? glm::vec3(state.input.x, 0.0f, state.input.z)
                                              : glm::vec3(0.0f);
            const auto flags = fresh ? static_cast<uint8_t>(state.input.y)
                                     : static_cast<uint8_t>(state.crouched ? nx3d::kMoveCrouch : 0);
            const bool jump = (flags & nx3d::kMoveJump) != 0;
            const bool crouch = (flags & nx3d::kMoveCrouch) != 0;
            const bool silentWalk = (flags & nx3d::kMoveSilentWalk) != 0;
            auto position = asGlm(user->getObject3D().getPosition());
            const glm::vec2 previousHorizontal(position.x, position.z);
            const bool wasCrouched = state.crouched;
            mapPhysics().advance(position, direction, jump, crouch, silentWalk, state.crouched,
                                 state.jumpHeld, state.grounded, state.verticalVelocity, dt);
            stanceChanged = state.crouched != wasCrouched;
            crouched = state.crouched;
            const bool moving = glm::distance(previousHorizontal,
                                              glm::vec2(position.x, position.z)) > 0.01f;
            animation = state.crouched ? "crouching" : !moving ? "idle"
                        : silentWalk ? "sneaking" : "running";
            animationChanged = animation != state.animation ||
                               now - state.lastAnimationBroadcast >= std::chrono::milliseconds(500);
            if (animationChanged)
            {
                state.animation = animation;
                state.lastAnimationBroadcast = now;
            }
            activeWeapon = state.activeWeapon;
            position.x = std::clamp(position.x, -nx3d::kMaxHorizontalPosition,
                                    nx3d::kMaxHorizontalPosition);
            position.z = std::clamp(position.z, -nx3d::kMaxHorizontalPosition,
                                    nx3d::kMaxHorizontalPosition);
            position.y = std::clamp(position.y, nx3d::kMinPlayerHeight,
                                    nx3d::kMaxPlayerHeight);
            user->getObject3D().setPosition({position.x, position.y, position.z});
            user->getObject3D().setYaw(state.yaw);
        }
        if (stanceChanged)
            publishStance(room, *user, crouched);
        if (animationChanged)
            publishAnimation(room, *user, animation, 0.0f, activeWeapon);
        publishPosition(room, *user);
    }
}

Authority::DroppedWeapon Authority::dropWeapon(uint64_t playerId, uint8_t slot,
                                               const std::string& type)
{
    const auto rule = weaponRule(type);
    if (!rule)
        return {};
    std::lock_guard lock(m_mutex);
    auto& player = m_players[playerId];
    auto& gun = slot == 1 ? player.primary : player.secondary;
    DroppedWeapon result{type, rule->magazine, rule->magazine * rule->reserveMagazines};
    if (gun.type == type)
    {
        result.magazine = gun.magazine;
        result.reserve = gun.reserve;
    }
    gun = {};
    return result;
}

void Authority::pickupWeapon(uint64_t playerId, uint8_t slot, const DroppedWeapon& weapon)
{
    std::lock_guard lock(m_mutex);
    auto& gun = slot == 1 ? m_players[playerId].primary : m_players[playerId].secondary;
    gun = {};
    gun.type = weapon.type;
    gun.magazine = weapon.magazine;
    gun.reserve = weapon.reserve;
}

bool Authority::equipWeapon(nexilis::server::Room& room, uint64_t playerId,
                            const std::string& weapon)
{
    auto* user = nexilis::server::ClientStorage::getClientById(playerId);
    if (!user || user->getRoomId() != room.getId() ||
        room.getPlayerHealth(playerId) <= 0.0f || room.getPlayerTeam(playerId).empty() ||
        (!weaponRule(weapon) && weapon != "grenade" && weapon != "smoke" &&
         weapon != "bomb" && weapon != "defuse_kit"))
        return false;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_players.find(playerId);
        if (it == m_players.end())
            return false;
        it->second.activeWeapon = weapon;
    }
    publishAnimation(room, *user, "equipped", 0.0f, weapon);
    return true;
}

void Authority::refill(uint64_t playerId)
{
    std::lock_guard lock(m_mutex);
    auto it = m_players.find(playerId);
    if (it == m_players.end())
        return;
    for (auto* gun : {&it->second.primary, &it->second.secondary})
    {
        const auto rule = weaponRule(gun->type);
        if (!rule)
            continue;
        gun->magazine = rule->magazine;
        gun->reserve = rule->magazine * rule->reserveMagazines;
        gun->reloadEnd = {};
    }
}

std::optional<nexilis::server::Room::ShotHit> Authority::fire(
        nexilis::server::Room& room, uint64_t shooterId,
        const nexilis::Vector3f& direction, uint8_t slot, uint8_t action,
        const std::string& weapon)
{
    auto* shooter = nexilis::server::ClientStorage::getClientById(shooterId);
    if (!shooter || shooter->getRoomId() != room.getId() ||
        room.getPlayerHealth(shooterId) <= 0.0f || room.getPlayerTeam(shooterId).empty() ||
        (slot != 1 && slot != 2 && slot != 3) || (action != 0 && action != 1))
        return std::nullopt;
    const auto rule = weaponRule(weapon);
    if (!rule)
        return std::nullopt;
    const auto now = std::chrono::steady_clock::now();
    bool reloadStarted = false;
    {
        std::lock_guard lock(m_mutex);
        auto it = m_players.find(shooterId);
        if (it == m_players.end())
            return std::nullopt;
        auto& state = it->second;
        WeaponState& gun = slot == 1 ? state.primary : slot == 2 ? state.secondary
                                                                 : state.knife;
        if (gun.type != weapon)
        {
            gun = WeaponState{};
            gun.type = weapon;
            gun.magazine = rule->magazine;
            gun.reserve = rule->magazine * rule->reserveMagazines;
        }
        if (gun.reloadEnd != std::chrono::steady_clock::time_point{} && now >= gun.reloadEnd)
        {
            const int amount = std::min(rule->magazine - gun.magazine, gun.reserve);
            gun.magazine += amount;
            gun.reserve -= amount;
            gun.reloadEnd = {};
        }
        if (action == 1)
        {
            if (gun.reloadEnd == std::chrono::steady_clock::time_point{} &&
                gun.magazine < rule->magazine && gun.reserve > 0)
            {
                gun.reloadEnd = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                              std::chrono::duration<float>(rule->reloadSeconds));
                reloadStarted = true;
                state.activeWeapon = weapon;
            }
        }
        else
        {
            const glm::vec3 aim = asGlm(direction);
            const float aimLength = glm::length(aim);
            if (!std::isfinite(aimLength) || aimLength < 0.5f || aimLength > 1.5f ||
                gun.reloadEnd != std::chrono::steady_clock::time_point{} ||
                (rule->magazine > 0 && gun.magazine <= 0))
                return std::nullopt;
            if (nexilis::server::GameplayLimits::get().getMaxShootingRate() > 0.0f &&
                now - gun.lastShot < std::chrono::duration<float>(60.0f / rule->roundsPerMinute - 0.002f))
                return std::nullopt;
            gun.lastShot = now;
            state.activeWeapon = weapon;
            if (rule->magazine > 0)
                --gun.magazine;
        }
    }

    if (action == 1)
    {
        if (reloadStarted)
            publishAnimation(room, *shooter, "reloading", rule->reloadSeconds, weapon);
        return std::nullopt;
    }
    publishAnimation(room, *shooter, "shooting", 0.16f, weapon);

    const glm::vec3 origin = asGlm(shooter->getObject3D().getPosition());
    const glm::vec3 aim = glm::normalize(asGlm(direction));
    const auto shooterTeam = room.getPlayerTeam(shooterId);
    float closest = rule->range;
    uint64_t targetId = 0;
    for (auto id : room.getClients())
    {
        if (id == shooterId || room.getPlayerHealth(id) <= 0.0f ||
            room.getPlayerTeam(id) == shooterTeam || !room.canDamage(shooterId, id))
            continue;
        auto* target = nexilis::server::ClientStorage::getClientById(id);
        if (!target)
            continue;
        const auto position = target->getObject3D().getPosition();
        bool targetCrouched = false;
        {
            std::lock_guard lock(m_mutex);
            if (auto it = m_players.find(id); it != m_players.end())
                targetCrouched = it->second.crouched;
        }
        const float distance = nx3d::playerHitDistance(origin, aim, asGlm(position),
                                                       target->getObject3D().getYaw(),
                                                       targetCrouched ? nx3d::kCrouchingEyeHeight
                                                                      : nx3d::kStandingEyeHeight);
        if (distance < closest)
        {
            closest = distance;
            targetId = id;
        }
    }
    if (!targetId || mapPhysics().blocks(origin, origin + aim * closest))
        return std::nullopt;
    return nexilis::server::Room::ShotHit{targetId, rule->damage};
}

void Authority::reconcile(const nexilis::server::Room& room)
{
    const std::unordered_set<uint64_t> present(room.getClients().begin(), room.getClients().end());
    std::lock_guard lock(m_mutex);
    std::erase_if(m_players, [&](const auto& entry)
                  { return !present.contains(entry.first); });
}

} // namespace nx3d::server::gamemode
