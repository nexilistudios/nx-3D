#include <spear/camera.hh>
#include <spear/movement_controller.hh>
#include <shared/brush_mesh.hh>
#include <shared/economy.hh>
#include <fstream>

#include <btBulletDynamicsCommon.h>

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>

int main()
{
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher(&config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &config);
    btStaticPlaneShape floor(btVector3(0.0f, 1.0f, 0.0f), -128.0f);
    btRigidBody::btRigidBodyConstructionInfo floorInfo(0.0f, nullptr, &floor);
    btRigidBody floorBody(floorInfo);
    world.addRigidBody(&floorBody);

    spear::Camera camera(glm::vec3(0.0f, -48.0f, 0.0f));
    spear::MovementController movement(camera, &world, 64.0f);
    movement.resetVerticalMotion();
    const std::unordered_map<SDL_Keycode, bool> keys{
            {SDLK_W, false}, {SDLK_S, false}, {SDLK_A, false},
            {SDLK_D, false}, {SDLK_SPACE, false}};

    // A half-second frame crosses the CT floor entirely. Ground detection
    // must still land on it instead of starting its ray below the surface.
    movement.processInput(keys, 0.5f);
    if (std::abs(camera.getPosition().y - (-64.0f)) > 0.01f || !movement.isOnGround())
    {
        std::cerr << "Player fell through the CT spawn floor\n";
        return 1;
    }

    movement.setMovementModifiers(40.0f, 0.4f, 0.5f);
    auto quietWalk = keys;
    quietWalk[SDLK_LSHIFT] = true;
    quietWalk[SDLK_W] = true;
    const auto beforeQuietWalk = camera.getPosition();
    movement.processInput(quietWalk, 0.1f);
    const auto quietDistance = glm::length(glm::vec2(camera.getPosition().x - beforeQuietWalk.x,
                                                     camera.getPosition().z - beforeQuietWalk.z));
    if (std::abs(quietDistance - camera.getSpeed() * 0.05f) > 0.01f)
    {
        std::cerr << "Shift did not slow local movement\n";
        return 1;
    }

    auto crouch = keys;
    crouch[SDLK_LCTRL] = true;
    crouch[SDLK_W] = true;
    const auto beforeCrouch = camera.getPosition();
    movement.processInput(crouch, 0.1f);
    const auto crouchDistance = glm::length(glm::vec2(camera.getPosition().x - beforeCrouch.x,
                                                      camera.getPosition().z - beforeCrouch.z));
    if (!movement.isCrouched() || std::abs(camera.getPosition().y - (-88.0f)) > 0.01f ||
        std::abs(crouchDistance - camera.getSpeed() * 0.04f) > 0.01f)
    {
        std::cerr << "Ctrl did not lower the camera and slow local movement\n";
        return 1;
    }

    btBoxShape ceilingShape(btVector3(30.0f, 7.0f, 30.0f));
    btTransform ceilingTransform;
    ceilingTransform.setIdentity();
    ceilingTransform.setOrigin(btVector3(0.0f, -73.0f, 0.0f));
    btDefaultMotionState ceilingMotion(ceilingTransform);
    btRigidBody::btRigidBodyConstructionInfo ceilingInfo(0.0f, &ceilingMotion, &ceilingShape);
    btRigidBody ceiling(ceilingInfo);
    world.addRigidBody(&ceiling);
    movement.processInput(keys, 0.016f);
    if (!movement.isCrouched())
    {
        std::cerr << "Player stood up through a low ceiling\n";
        return 1;
    }
    world.removeRigidBody(&ceiling);
    movement.processInput(keys, 0.016f);
    if (movement.isCrouched() || std::abs(camera.getPosition().y - (-64.0f)) > 0.01f)
    {
        std::cerr << "Player could not stand after leaving a low ceiling\n";
        return 1;
    }

    btBoxShape stairShape(btVector3(12.0f, 8.0f, 40.0f));
    btTransform stairTransform;
    stairTransform.setIdentity();
    stairTransform.setOrigin(btVector3(35.0f, -120.0f, 0.0f));
    btDefaultMotionState stairMotion(stairTransform);
    btRigidBody::btRigidBodyConstructionInfo stairInfo(0.0f, &stairMotion, &stairShape);
    btRigidBody stair(stairInfo);
    world.addRigidBody(&stair);

    btBoxShape wallShape(btVector3(10.0f, 50.0f, 40.0f));
    btTransform wallTransform;
    wallTransform.setIdentity();
    wallTransform.setOrigin(btVector3(90.0f, -78.0f, 0.0f));
    btDefaultMotionState wallMotion(wallTransform);
    btRigidBody::btRigidBodyConstructionInfo wallInfo(0.0f, &wallMotion, &wallShape);
    btRigidBody wall(wallInfo);
    world.addRigidBody(&wall);

    spear::Camera stairsCamera(glm::vec3(0.0f, -64.0f, 0.0f),
                               glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f, 100.0f);
    spear::MovementController stairsMovement(stairsCamera, &world, 64.0f);
    auto walking = keys;
    walking[SDLK_W] = true;
    for (int frame = 0; frame < 12; ++frame)
        stairsMovement.processInput(walking, 0.016f);
    if (stairsCamera.getPosition().x < 16.0f ||
        std::abs(stairsCamera.getPosition().y - (-48.0f)) > 0.01f ||
        !stairsMovement.isOnGround())
    {
        std::cerr << "Player could not step onto a low stair: x="
                  << stairsCamera.getPosition().x << " y=" << stairsCamera.getPosition().y
                  << " grounded=" << stairsMovement.isOnGround() << '\n';
        return 1;
    }

    stairsMovement.processInput(keys, 0.016f);
    if (std::abs(stairsCamera.getPosition().y - (-48.0f)) > 0.01f)
    {
        std::cerr << "Player slipped off the stair edge while standing still\n";
        return 1;
    }

    for (int frame = 0; frame < 4; ++frame)
        stairsMovement.processInput(walking, 0.016f);

    stairsMovement.processInput(walking, 0.4f);
    if (stairsCamera.getPosition().x < 59.0f ||
        std::abs(stairsCamera.getPosition().y - (-64.0f)) > 0.01f ||
        !stairsMovement.isOnGround())
    {
        std::cerr << "Player did not follow the floor after the stair: x="
                  << stairsCamera.getPosition().x << " y=" << stairsCamera.getPosition().y
                  << " grounded=" << stairsMovement.isOnGround() << '\n';
        return 1;
    }

    stairsMovement.processInput(walking, 0.5f);
    if (stairsCamera.getPosition().x >= 80.0f)
    {
        std::cerr << "Player passed through a wall\n";
        return 1;
    }

    auto sliding = walking;
    sliding[SDLK_D] = true;
    stairsMovement.processInput(sliding, 0.2f);
    if (stairsCamera.getPosition().x >= 80.0f || stairsCamera.getPosition().z < 5.0f)
    {
        std::cerr << "Player could not slide along a wall\n";
        return 1;
    }

    auto jumping = keys;
    jumping[SDLK_SPACE] = true;
    const float groundEyeY = stairsCamera.getPosition().y;
    stairsMovement.processInput(jumping, 0.05f);
    if (stairsCamera.getPosition().y <= groundEyeY || stairsMovement.isOnGround())
    {
        std::cerr << "Jump was snapped back to the ground\n";
        return 1;
    }

    // Walking uphill must follow a walkable slope instead of losing ground
    // contact and falling through the terrain.
    const btVector3 slopeNormal(-0.342020f, 0.939693f, 0.0f);
    btStaticPlaneShape slopeShape(slopeNormal, slopeNormal.dot(btVector3(200.0f, 0.0f, 200.0f)));
    btRigidBody::btRigidBodyConstructionInfo slopeInfo(0.0f, nullptr, &slopeShape);
    btRigidBody slope(slopeInfo);
    world.addRigidBody(&slope);

    spear::Camera slopeCamera(glm::vec3(200.0f, 64.0f, 200.0f),
                              glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f, 250.0f);
    spear::MovementController slopeMovement(slopeCamera, &world, 64.0f);
    for (int frame = 0; frame < 20; ++frame)
        slopeMovement.processInput(walking, 0.016f);
    if (slopeCamera.getPosition().x < 270.0f ||
        slopeCamera.getPosition().y < 85.0f || !slopeMovement.isOnGround())
    {
        std::cerr << "Player did not walk uphill: x=" << slopeCamera.getPosition().x
                  << " y=" << slopeCamera.getPosition().y
                  << " grounded=" << slopeMovement.isOnGround() << '\n';
        return 1;
    }

    auto downhill = keys;
    downhill[SDLK_S] = true;
    for (int frame = 0; frame < 20; ++frame)
        slopeMovement.processInput(downhill, 0.016f);
    if (slopeCamera.getPosition().x > 210.0f ||
        std::abs(slopeCamera.getPosition().y - 64.0f) > 4.0f ||
        !slopeMovement.isOnGround())
    {
        std::cerr << "Player did not follow terrain downhill: x="
                  << slopeCamera.getPosition().x << " y=" << slopeCamera.getPosition().y
                  << " grounded=" << slopeMovement.isOnGround() << '\n';
        return 1;
    }

    std::ifstream mapFile(std::string(PROJECT_ROOT) + "/assets/" + nx3d::kGeneratedMapPath);
    if (!mapFile) { std::cerr << "Cannot load generated map\n"; return 1; }
    btTriangleMesh mapMesh;
    const auto mapGeometry = nx3d::buildBrushMesh(nx3d::readBrushMap(mapFile));
    for (const auto& vertices : mapGeometry)
    {
        for (std::size_t i = 0; i < vertices.size(); i += 3)
        {
            const auto& a = vertices[i].position;
            const auto& b = vertices[i+1].position;
            const auto& c = vertices[i+2].position;
            mapMesh.addTriangle({a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z});
        }
    }
    btBvhTriangleMeshShape mapShape(&mapMesh, true);
    btRigidBody::btRigidBodyConstructionInfo mapInfo(0.0f, nullptr, &mapShape);
    btRigidBody mapBody(mapInfo);
    btDbvtBroadphase mapBroadphase;
    btDiscreteDynamicsWorld mapWorld(&dispatcher, &mapBroadphase, &solver, &config);
    mapWorld.addRigidBody(&mapBody);
    const std::array<glm::vec3, 10> spawns{{
            {355.0f, -48.0f, -2364.0f}, {178.0f, -48.0f, -2408.0f},
            {381.0f, -48.0f, -2470.0f}, {180.0f, -48.0f, -2368.0f},
            {123.0f, -48.0f, -2519.0f}, {-592.0f, 192.0f, 764.0f},
            {-834.0f, 192.0f, 797.0f}, {-561.0f, 192.0f, 771.0f},
            {-801.0f, 192.0f, 777.0f}, {-705.0f, 204.0f, 947.0f}}};
    for (const auto& spawn : spawns)
    {
        spear::Camera spawnCamera(spawn);
        spear::MovementController spawnMovement(spawnCamera, &mapWorld, 64.0f);
        spawnMovement.resetVerticalMotion();
        for (int frame = 0; frame < 180; ++frame)
            spawnMovement.processInput(keys, 1.0f / 60.0f);
        if (!spawnMovement.isOnGround() || spawnCamera.getPosition().y < -256.0f)
        {
            std::cerr << "Spawn fell through map: x=" << spawn.x << " z=" << spawn.z
                      << " y=" << spawnCamera.getPosition().y << '\n';
            return 1;
        }
    }

    // A standing capsule must pass the generated gates and CT underpass.
    const auto plan = [](float x, float z, float floor)
    { return btVector3((x - 795.0f) * 3.2f, floor + 38.4f, (z - 1160.0f) * 3.2f); };
    const std::array<std::array<btVector3, 2>, 8> passages{{
            {{plan(672,620,-112), plan(672,680,-112)}},
            {{plan(350,475,0), plan(365,475,0)}},
            {{plan(365,475,0), plan(430,475,-112)}},
            {{plan(1020,1120,128), plan(1020,1095,128)}},
            {{plan(1020,1095,128), plan(1020,945,0)}},
            {{plan(1020,945,0), plan(1020,920,0)}},
            {{plan(160,585,0), plan(160,735,0)}},
            {{plan(860,480,-112), plan(1100,480,-112)}}}};
    btCapsuleShape standingCapsule(7.68f, 57.6f - 2.0f * 7.68f);
    for (std::size_t i = 0; i < passages.size(); ++i)
    {
        btTransform from, to;
        from.setIdentity(); to.setIdentity();
        from.setOrigin(passages[i][0]); to.setOrigin(passages[i][1]);
        btCollisionWorld::ClosestConvexResultCallback hit(from.getOrigin(), to.getOrigin());
        mapWorld.convexSweepTest(&standingCapsule, from, to, hit);
        if (hit.hasHit() && hit.m_closestHitFraction < .99f)
        {
            std::cerr << "Generated map passage " << i << " blocks a standing player\n";
            return 1;
        }
    }
    return 0;
}
