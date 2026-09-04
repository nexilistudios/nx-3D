#include <spear/spear_vulkan.hh>

#include <nexilis/client/create_client_config.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/cmd_line_options.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/room_info.hh>
#include <nexilis/start_client.hh>
#include <nexilis/tcp_client.hh>
#include <nexilis/udp_client.hh>

#include <btBulletDynamicsCommon.h>

#include <client/crosshair/crosshair.hh>
#include <client/gun/first_person_gun.hh>

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{

enum class State
{
    ServerInput,
    Connecting,
    Lobby,
    Joining,
    TeamSelect,
    Game
};

} // namespace

int main(int argc, char* argv[])
{
    const std::string window_name = "nx_3D game";
    const spear::BaseWindow::Size window_size = {820, 640};
    std::atomic<bool> ready = false;
    std::mutex mtx;
    std::vector<nexilis::RoomInfo> rooms;

    // Parse --server argument
    nexilis::CmdLineOptions cmdLine(argc, argv);
    std::string serverAddress = cmdLine.getValue<std::string>("-server", "127.0.0.1");
    std::string serverAddressInput = "127.0.0.1";
    bool serverInputCleared = false;
    std::string lastServerDisplay;
    State currentState;

    if (argc > 1)
    {
        currentState = State::Connecting;
    }
    else
    {
        currentState = State::ServerInput;
    }

    using packet = nexilis::client::Packet;

    nexilis::ProtocolManager protocolManager;
    nexilis::client::ClientAPI client_api(nexilis::client::createClientConfig(serverAddress, "password"));

    auto tcp_client = protocolManager.createProtocol<nexilis::TCPClient>(client_api);

    if (currentState == State::Connecting)
    {
        auto start_client = nexilis::startClient(client_api, tcp_client, rooms, ready, mtx);
        start_client.detach();
    }

    auto udp_client = protocolManager.createProtocol<nexilis::UDPClient>(client_api);

    spear::VulkanWindow window(window_name, window_size);
    auto w_size = window.getSize();
    std::cout << "Window size x: " << w_size.x << " y: " << w_size.y << std::endl;

    spear::Camera camera(glm::vec3(-1600.0f, 64.0f, -2600.0f), glm::vec3(0.f, 1.f, 0.f), 90.0f, 0.f, 250.f);
    spear::SceneManager scene_manager;

    namespace blt = spear::physics::bullet;
    namespace vk = spear::rendering::vulkan;

    blt::World bullet_world;
    auto shared_bullet_world = std::make_shared<btDiscreteDynamicsWorld>(*bullet_world.getDynamicsWorld());
    spear::MovementController movement_controller(camera, shared_bullet_world.get(), 64.0f);
    auto default_size = glm::vec3(1.0f, 1.0f, 1.0f);

    vk::Renderer renderer(window);
    renderer.init();
    renderer.setBackgroundColor(0.1f, 0.1f, 0.2f, 1.0f);
    renderer.setCamera(&camera);

    VkDevice device = renderer.getDevice();
    VkPhysicalDevice physDevice = renderer.getPhysicalDevice();

    // --- Descriptor pool + layout ---
    VkDescriptorPool descriptorPool = vk::Texture::createDescriptorPool(device, 256);
    VkDescriptorSetLayout descriptorSetLayout = vk::Texture::createDescriptorSetLayout(device);

    renderer.initializeTexturedPipeline(descriptorSetLayout);
    renderer.initializeUIPipeline(descriptorSetLayout);

    // --- Textures ---
    auto texture = std::make_shared<vk::STBTexture>(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue());
    texture->loadFromFile(spear::getAssetPath("wallnut.jpg"));

    auto niilo_texture = std::make_shared<vk::STBTexture>(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue());
    niilo_texture->loadFromFile(spear::getAssetPath("niilo.jpg"));

    // --- Scenes ---
    // Lobby scene: empty (just background)
    auto lobby_objects = spear::Scene::Container{};
    auto lobby_function = [](spear::Scene::Container&) {};
    auto lobby_scene_id = spear::createScene(lobby_objects, lobby_function, scene_manager);
    scene_manager.getSceneById(lobby_scene_id)->setName("lobby");

    // Game scene: de_dust2 map
    auto dust2_model = std::make_shared<vk::OBJModel>(
            device, physDevice,
            renderer.getCommandPool(), renderer.getGraphicsQueue(),
            "/home/valtteri/code/nx-3D/assets/de_dust2/source/de_dust2.obj", "/home/valtteri/code/nx-3D/assets/de_dust2/source/de_dust2.mtl",
            descriptorPool, descriptorSetLayout,
            blt::ObjectData(shared_bullet_world, 0.0f,
                            glm::vec3(0.0f, 0.0f, 0.0f), default_size));
    // Rotate the map: de_dust2 OBJ uses Z as vertical (CS:GO convention),
    // but the engine uses Y as vertical (OpenGL convention).
    dust2_model->rotate(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // Build triangle mesh from OBJ for ground collision (leaked intentionally at exit)
    auto& loader = dust2_model->getLoader();
    const auto& vertices = loader.getVertices();
    const auto& facesByMaterial = loader.getFacesByMaterial();
    auto* triMesh = new btTriangleMesh();
    for (const auto& faces : facesByMaterial)
    {
        for (const auto& face : faces)
        {
            if (face.vertexIndices.size() >= 3)
            {
                auto& v0 = vertices[face.vertexIndices[0]];
                auto& v1 = vertices[face.vertexIndices[1]];
                auto& v2 = vertices[face.vertexIndices[2]];
                // OBJ (x, y, z-up) -> OpenGL (x, y-up, z): (x, z, -y)
                btVector3 b0(v0.x, v0.z, -v0.y);
                btVector3 b1(v1.x, v1.z, -v1.y);
                btVector3 b2(v2.x, v2.z, -v2.y);
                triMesh->addTriangle(b0, b1, b2);
                if (face.vertexIndices.size() == 4)
                {
                    auto& v3 = vertices[face.vertexIndices[3]];
                    btVector3 b3(v3.x, v3.z, -v3.y);
                    triMesh->addTriangle(b0, b2, b3);
                }
            }
        }
    }
    auto* meshShape = new btBvhTriangleMeshShape(triMesh, true);
    btTransform groundTransform;
    groundTransform.setIdentity();
    auto* meshMotionState = new btDefaultMotionState(groundTransform);
    btRigidBody::btRigidBodyConstructionInfo meshRbInfo(0.0f, meshMotionState, meshShape);
    auto* meshRigidBody = new btRigidBody(meshRbInfo);
    shared_bullet_world->addRigidBody(meshRigidBody);

    auto ak47Texture = std::make_shared<vk::STBTexture>(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue());
    ak47Texture->loadFromFile("/home/valtteri/code/nx-3D/assets/ak47/textures/low_AK47_BaseColor.png");

    // White 1x1 texture for crosshair quads
    auto crosshairTexture = std::make_shared<vk::STBTexture>(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue());
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    crosshairTexture->loadFromRGBA(whitePixel, 1, 1);

    // clang-format off
    auto game_objects = spear::Scene::Container{
        dust2_model
    };
    // clang-format on
    auto game_function = [](spear::Scene::Container&) {};
    auto game_scene_id = spear::createScene(game_objects, game_function, scene_manager);
    scene_manager.getSceneById(game_scene_id)->setName("game");

    // --- Weapon state ---
    bool weaponPickedUp = false;
    uint64_t pickedUpItemId = 0;
    glm::vec3 pickedUpItemSize{1.0f};
    std::string pickedUpItemType;
    std::string pickedUpItemFilepath;
    int dropCooldown = 0;
    int health = 100;
    std::shared_ptr<nx3d::client::gun::FirstPersonGun> firstPersonGun;
    std::shared_ptr<nx3d::client::Crosshair> crosshair;

    // Start in lobby
    scene_manager.loadScene(lobby_scene_id);
    spear::Time time_interface;

    // --- UI Renderer ---
    std::string fontPath = "/usr/share/fonts/TTF/FiraCode-Retina.ttf";
    spear::ui::vulkan::UIRenderer uiRenderer(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 24);

    spear::ui::vulkan::Text titleText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 32);
    titleText.setString("nx-3D Lobby");
    titleText.setColor(SDL_Color{0, 200, 255, 255});
    titleText.setPosition(glm::vec2(-0.8f, 0.7f));

    spear::ui::vulkan::Text statusText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 20);
    statusText.setString("Connecting to server...");
    statusText.setColor(SDL_Color{200, 200, 200, 255});
    statusText.setPosition(glm::vec2(-0.8f, 0.5f));

    spear::ui::vulkan::Text instructionsText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 16);
    instructionsText.setString("");
    instructionsText.setPosition(glm::vec2(-0.8f, -0.8f));

    // HUD texts (used during Game state, bottom-left corner)
    spear::ui::vulkan::Text healthText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 24);
    healthText.setString("HP: 100");
    healthText.setColor(SDL_Color{255, 255, 255, 255});
    healthText.setPosition(glm::vec2(-0.98f, -0.98f));

    spear::ui::vulkan::Text weaponHudText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 20);
    weaponHudText.setString("");
    weaponHudText.setColor(SDL_Color{0, 255, 0, 255});
    weaponHudText.setPosition(glm::vec2(-0.98f, -0.88f));

    // Hitmarker overlay (shown briefly when dealing damage)
    spear::ui::vulkan::Text hitmarkerText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 28);
    hitmarkerText.setString("");
    hitmarkerText.setColor(SDL_Color{255, 255, 255, 255});
    hitmarkerText.setPosition(glm::vec2(-0.025f, -0.025f));
    hitmarkerText.setScale(0.003f);
    int hitmarkerFrames = 0;

    // Team select UI
    spear::ui::vulkan::Text ctButtonText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 24);
    ctButtonText.setString("Counter Terrorist");
    ctButtonText.setColor(SDL_Color{255, 255, 255, 255});
    ctButtonText.setPosition(glm::vec2(-0.72f, 0.03f));

    spear::ui::vulkan::Text tButtonText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 24);
    tButtonText.setString("Terrorist");
    tButtonText.setColor(SDL_Color{255, 255, 255, 255});
    tButtonText.setPosition(glm::vec2(0.35f, 0.03f));

    spear::ui::vulkan::Text teamSelectTitle(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 32);
    teamSelectTitle.setString("Choose Your Team");
    teamSelectTitle.setColor(SDL_Color{255, 255, 255, 255});
    teamSelectTitle.setPosition(glm::vec2(-0.3f, 0.4f));

    spear::ui::vulkan::Text teamSelectInstructions(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 16);
    teamSelectInstructions.setString("1 = CT    2 = T    |    Click a team to join");
    teamSelectInstructions.setColor(SDL_Color{200, 200, 200, 255});
    teamSelectInstructions.setPosition(glm::vec2(-0.25f, -0.3f));

    // Team display (bottom right corner, shown during Game state)
    spear::ui::vulkan::Text teamDisplayText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 20);
    teamDisplayText.setString("");
    teamDisplayText.setColor(SDL_Color{255, 255, 255, 255});
    teamDisplayText.setPosition(glm::vec2(0.65f, -0.95f));

    // Server address input UI
    spear::ui::vulkan::Text serverInputLabel(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 24);
    serverInputLabel.setString("Server address:");
    serverInputLabel.setColor(SDL_Color{255, 255, 255, 255});
    serverInputLabel.setPosition(glm::vec2(-0.8f, 0.3f));

    spear::ui::vulkan::Text serverAddressText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 24);
    serverAddressText.setString("|");
    serverAddressText.setColor(SDL_Color{0, 200, 255, 255});
    serverAddressText.setPosition(glm::vec2(-0.8f, 0.1f));

    spear::ui::vulkan::Text serverInputInstructions(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 16);
    serverInputInstructions.setString("Type address and press Enter   |   ESC: Quit");
    serverInputInstructions.setColor(SDL_Color{200, 200, 200, 255});
    serverInputInstructions.setPosition(glm::vec2(-0.8f, -0.8f));

    std::string chosenTeam;

    std::vector<glm::vec3> ctSpawns = {
        {355.0f, -64.0f, -2364.0f},
        {178.0f, -64.0f, -2408.0f}
    };
    std::vector<glm::vec3> tSpawns = {
        {-592.0f, 192.0f, 764.0f},
        {-834.0f, 192.0f, 797.0f}
    };

    // Menu list for rooms
    spear::ui::BaseMenuList* roomMenu = nullptr;

    renderer.setUIRenderer(&uiRenderer);

    uiRenderer.addExternalText(titleText);
    uiRenderer.addExternalText(statusText);
    uiRenderer.addExternalText(instructionsText);

    bool menuPopulated = false;
    std::atomic<bool> joiningRoom = false;
    uint64_t selectedRoomId = 0;

    if (currentState == State::ServerInput)
    {
        uiRenderer.addExternalText(serverInputLabel);
        uiRenderer.addExternalText(serverAddressText);
        uiRenderer.addExternalText(serverInputInstructions);
        SDL_StartTextInput(window.getSDLWindow());
    }

    // --- Clean quit helper ---
    auto cleanQuit = [&tcp_client, &currentState, &device, &descriptorPool, &descriptorSetLayout, &window, &client_api]()
    {
        SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), false);
        if (client_api.clientInRoom())
        {
            tcp_client.sendMessage(packet::Room::Management::leave(client_api));
        }
        tcp_client.stop();
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        exit(0);
    };

    // --- Event Handlers ---
    spear::EventHandler eventHandler;

    eventHandler.handleInput(SDLK_ESCAPE, [&cleanQuit]()
                             { cleanQuit(); });

    eventHandler.handleInput(SDLK_P, [&ready, &tcp_client, &camera, &currentState, &client_api]()
                             {
        if (currentState == State::Game && ready && client_api.clientInRoom())
        {
            auto cam_pos = camera.getPosition();
            auto cam_front = camera.getFront();
            glm::vec3 spawn_pos = cam_pos + cam_front;
            auto pos = nexilis::Vector3f({spawn_pos.x, spawn_pos.y, spawn_pos.z});
            auto dim = nexilis::Vector3f({1.0f, 1.0f, 1.0f});
            tcp_client.sendMessage(
                    packet::Room::Object3D::create(client_api, pos, dim, ""));
        } });

    eventHandler.registerCallback(SDL_EVENT_QUIT, [&cleanQuit](const SDL_Event&)
                                  { cleanQuit(); });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_MOTION, [&camera](const SDL_Event& event)
                                  { camera.rotate(event.motion.xrel, event.motion.yrel); });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_BUTTON_DOWN,
                                  [&currentState, &weaponPickedUp, &camera, &tcp_client, &client_api](const SDL_Event& event)
                                  {
                                      if (currentState == State::Game && weaponPickedUp && event.button.button == SDL_BUTTON_LEFT)
                                      {
                                          glm::vec3 rayOrigin = camera.getPosition();
                                          glm::vec3 rayDir = glm::normalize(camera.getFront());

                                          auto room_id = client_api.clientRoomId();
                                          auto my_id = client_api.getClientId();
                                          auto players = client_api.getRemotePlayersSnapshot(room_id, my_id);

                                          float closestDist = std::numeric_limits<float>::max();
                                          uint64_t hitTargetId = 0;

                                          for (auto& player : players)
                                          {
                                              glm::vec3 center(player.x, player.y, player.z);
                                              glm::vec3 halfExtents(5.0f, 5.0f, 5.0f);
                                              glm::vec3 boxMin = center - halfExtents;
                                              glm::vec3 boxMax = center + halfExtents;

                                              float tmin = -std::numeric_limits<float>::max();
                                              float tmax = std::numeric_limits<float>::max();
                                              bool miss = false;

                                              for (int i = 0; i < 3; i++)
                                              {
                                                  float origin_i = (&rayOrigin.x)[i];
                                                  float dir_i = (&rayDir.x)[i];
                                                  float min_i = (&boxMin.x)[i];
                                                  float max_i = (&boxMax.x)[i];

                                                  if (std::abs(dir_i) < 1e-8f)
                                                  {
                                                      if (origin_i < min_i || origin_i > max_i)
                                                      {
                                                          miss = true;
                                                          break;
                                                      }
                                                  }
                                                  else
                                                  {
                                                      float invD = 1.0f / dir_i;
                                                      float t1 = (min_i - origin_i) * invD;
                                                      float t2 = (max_i - origin_i) * invD;
                                                      if (t1 > t2)
                                                          std::swap(t1, t2);
                                                      tmin = std::max(tmin, t1);
                                                      tmax = std::min(tmax, t2);
                                                      if (tmin > tmax)
                                                      {
                                                          miss = true;
                                                          break;
                                                      }
                                                  }
                                              }

                                              if (!miss && tmin >= 0.0f && tmin < closestDist)
                                              {
                                                  closestDist = tmin;
                                                  hitTargetId = player.id;
                                              }
                                          }

                                          if (hitTargetId != 0)
                                          {
                                              tcp_client.sendMessage(
                                                      packet::Room::Player3D::shoot(client_api, hitTargetId, 35.0f));
                                          }
                                      }
                                  });

    eventHandler.registerCallback(SDL_EVENT_WINDOW_RESIZED, [&window, &renderer](const SDL_Event&)
                                  {
        window.resize();
        auto s = window.getSize();
        renderer.setViewPort(s.x, s.y); });

    renderer.setScene(scene_manager.getCurrentScene());

    std::unordered_map<uint64_t, std::shared_ptr<vk::TexturedCube>> remote_players;
    std::unordered_map<uint64_t, std::shared_ptr<vk::TexturedCube>> remote_objects;
    std::unordered_map<uint64_t, std::shared_ptr<vk::TexturedCube>> remote_game_items;
    std::vector<std::shared_ptr<vk::TexturedCube>> pendingDestroy[3];
    int frameCount = 0;

    eventHandler.handleInput(SDLK_G, [&]()
                             {
        if (currentState == State::Game && weaponPickedUp)
        {
            weaponPickedUp = false;

            if (crosshair)
            {
                vkDeviceWaitIdle(device);
                crosshair.reset();
                uiRenderer.setOverlayCallback(nullptr);
            }

            scene_manager.getCurrentScene()->removeObject(firstPersonGun->getId());
            vkDeviceWaitIdle(device);
            firstPersonGun.reset();

            tcp_client.sendMessage(
                    packet::Room::GameItem::destroy(client_api,  pickedUpItemId));

            glm::vec3 dropPos = camera.getPosition() + camera.getFront() * 300.0f;
            auto pos = nexilis::Vector3f({dropPos.x, dropPos.y, dropPos.z});
            auto dim = nexilis::Vector3f({pickedUpItemSize.x, pickedUpItemSize.y, pickedUpItemSize.z});
            tcp_client.sendMessage(
                    packet::Room::GameItem::create(client_api, pos, dim, pickedUpItemType, "on_ground", pickedUpItemFilepath));

            weaponHudText.setString("");
            dropCooldown = 60;
        } });

    glm::vec3 prevCamPos = camera.getPosition();

    while (true)
    {
        pendingDestroy[frameCount % 3].clear();
        frameCount++;
        if (dropCooldown > 0)
            dropCooldown--;
        if (hitmarkerFrames > 0)
        {
            hitmarkerFrames--;
            hitmarkerText.setString(hitmarkerFrames > 0 ? "X" : "");
        }

        float delta_time = time_interface.getDeltaTime();
        time_interface.updateFromMain(delta_time);

        // --- State machine ---
        if (currentState == State::Connecting && ready && !menuPopulated)
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (!rooms.empty())
            {
                roomMenu = &uiRenderer.createMenuList();
                roomMenu->setPosition(glm::vec2(-0.8f, 0.3f));
                roomMenu->setSpacing(40.0f);
                for (auto& room : rooms)
                    roomMenu->addItem(room.getName());
                statusText.setString("Select a room and press Enter to join");
                instructionsText.setString("Arrow keys: Navigate   |   Enter: Join   |   ESC: Quit");
            }
            else
            {
                statusText.setString("No rooms available");
            }
            menuPopulated = true;
            currentState = State::Lobby;
            udp_client.start();
        }

        if (currentState == State::Joining && client_api.clientInRoom())
        {
            // Keep lobby scene (dark background) for team select
            // Switch UI from lobby to team select
            // Switch UI from lobby to team select
            uiRenderer.clear();
            uiRenderer.addExternalText(teamSelectTitle);
            uiRenderer.addExternalText(teamSelectInstructions);
            uiRenderer.addExternalText(ctButtonText);
            uiRenderer.addExternalText(tButtonText);
            renderer.setUIRenderer(&uiRenderer);
            currentState = State::TeamSelect;
        }

        // --- Event handling ---
        if (currentState == State::ServerInput)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    cleanQuit();
                }
                if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    if (event.key.key == SDLK_ESCAPE)
                    {
                        cleanQuit();
                    }
                    else if (event.key.key == SDLK_BACKSPACE)
                    {
                        if (!serverAddressInput.empty())
                        {
                            serverInputCleared = true;
                            serverAddressInput.pop_back();
                        }
                    }
                    else if (event.key.key == SDLK_RETURN)
                    {
                        serverAddress = serverAddressInput.empty() ? "127.0.0.1" : serverAddressInput;

                        tcp_client.stop();
                        client_api = nexilis::client::ClientAPI(
                                nexilis::client::createClientConfig(serverAddress, "password"));
                        tcp_client = protocolManager.createProtocol<nexilis::TCPClient>(client_api);
                        auto start_client = nexilis::startClient(client_api, tcp_client, rooms, ready, mtx);
                        start_client.detach();
                        udp_client = protocolManager.createProtocol<nexilis::UDPClient>(client_api);

                        uiRenderer.clear();
                        uiRenderer.addExternalText(titleText);
                        uiRenderer.addExternalText(statusText);
                        uiRenderer.addExternalText(instructionsText);
                        currentState = State::Connecting;
                    }
                }
                if (event.type == SDL_EVENT_TEXT_INPUT)
                {
                    if (!serverInputCleared)
                    {
                        serverAddressInput.clear();
                        serverInputCleared = true;
                    }
                    serverAddressInput += event.text.text;
                }
            }

            // Blinking cursor - only update when the displayed text changes
            // to avoid re-allocating a descriptor set on every frame.
            std::string displayText = serverAddressInput;
            if ((frameCount / 30) % 2 == 0)
                displayText += "|";
            else
                displayText += " ";
            if (displayText != lastServerDisplay)
            {
                lastServerDisplay = displayText;
                serverAddressText.setString(displayText);
            }
        }
        else if (currentState == State::Lobby || currentState == State::Connecting ||
            currentState == State::TeamSelect)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    cleanQuit();
                }
                if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    if (event.key.key == SDLK_ESCAPE)
                    {
                        cleanQuit();
                    }
                    if (currentState == State::Lobby && roomMenu && !joiningRoom)
                    {
                        if (event.key.key == SDLK_UP)
                            roomMenu->selectPrevious();
                        else if (event.key.key == SDLK_DOWN)
                            roomMenu->selectNext();
                        else if (event.key.key == SDLK_RETURN)
                        {
                            std::lock_guard<std::mutex> lock(mtx);
                            if (!rooms.empty())
                            {
                                int idx = roomMenu->getSelectedIndex();
                                if (idx >= 0 && idx < static_cast<int>(rooms.size()))
                                {
                                    selectedRoomId = rooms[idx].getId();
                                    tcp_client.sendMessage(
                                            packet::Room::Management::join(client_api, selectedRoomId));
                                    joiningRoom = true;
                                    statusText.setString("Joining room...");
                                    currentState = State::Joining;
                                }
                            }
                        }
                    }
                    if (currentState == State::TeamSelect)
                    {
                        if (event.key.key == SDLK_1)
                        {
                            chosenTeam = "Counter Terrorist";
                            teamDisplayText.setString(chosenTeam);
                            auto spawn = ctSpawns[rand() % ctSpawns.size()];
                            camera.setPosition(spawn);
                            scene_manager.loadScene(game_scene_id);
                            renderer.setScene(scene_manager.getCurrentScene());
                            uiRenderer.clear();
                            uiRenderer.addExternalText(healthText);
                            uiRenderer.addExternalText(weaponHudText);
                            uiRenderer.addExternalText(hitmarkerText);
                            uiRenderer.addExternalText(teamDisplayText);
                            renderer.setUIRenderer(&uiRenderer);
                            SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
                            currentState = State::Game;
                        }
                        else if (event.key.key == SDLK_2)
                        {
                            chosenTeam = "Terrorist";
                            teamDisplayText.setString(chosenTeam);
                            auto spawn = tSpawns[rand() % tSpawns.size()];
                            camera.setPosition(spawn);
                            scene_manager.loadScene(game_scene_id);
                            renderer.setScene(scene_manager.getCurrentScene());
                            uiRenderer.clear();
                            uiRenderer.addExternalText(healthText);
                            uiRenderer.addExternalText(weaponHudText);
                            uiRenderer.addExternalText(hitmarkerText);
                            uiRenderer.addExternalText(teamDisplayText);
                            renderer.setUIRenderer(&uiRenderer);
                            SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
                            currentState = State::Game;
                        }
                    }
                }
                if (event.type == SDL_EVENT_WINDOW_RESIZED)
                {
                    window.resize();
                    auto s = window.getSize();
                    renderer.setViewPort(s.x, s.y);
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && currentState == State::TeamSelect)
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        int live_w = 0, live_h = 0;
                        SDL_GetWindowSize(window.getSDLWindow(), &live_w, &live_h);
                        float mx = (static_cast<float>(event.button.x) / static_cast<float>(live_w ? live_w : 1)) * 2.0f - 1.0f;
                        float my = 1.0f - (static_cast<float>(event.button.y) / static_cast<float>(live_h ? live_h : 1)) * 2.0f;

                        if (mx >= -0.8f && mx <= -0.15f && my >= 0.0f && my <= 0.15f)
                        {
                            chosenTeam = "Counter Terrorist";
                            teamDisplayText.setString(chosenTeam);
                            auto spawn = ctSpawns[rand() % ctSpawns.size()];
                            camera.setPosition(spawn);
                            scene_manager.loadScene(game_scene_id);
                            renderer.setScene(scene_manager.getCurrentScene());
                            uiRenderer.clear();
                            uiRenderer.addExternalText(healthText);
                            uiRenderer.addExternalText(weaponHudText);
                            uiRenderer.addExternalText(hitmarkerText);
                            uiRenderer.addExternalText(teamDisplayText);
                            renderer.setUIRenderer(&uiRenderer);
                            SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
                            currentState = State::Game;
                        }
                        else if (mx >= 0.15f && mx <= 0.8f && my >= 0.0f && my <= 0.15f)
                        {
                            chosenTeam = "Terrorist";
                            teamDisplayText.setString(chosenTeam);
                            auto spawn = tSpawns[rand() % tSpawns.size()];
                            camera.setPosition(spawn);
                            scene_manager.loadScene(game_scene_id);
                            renderer.setScene(scene_manager.getCurrentScene());
                            uiRenderer.clear();
                            uiRenderer.addExternalText(healthText);
                            uiRenderer.addExternalText(weaponHudText);
                            uiRenderer.addExternalText(hitmarkerText);
                            uiRenderer.addExternalText(teamDisplayText);
                            renderer.setUIRenderer(&uiRenderer);
                            SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
                            currentState = State::Game;
                        }
                    }
                }
                if (event.type == SDL_EVENT_MOUSE_MOTION && currentState == State::Game)
                {
                    camera.rotate(event.motion.xrel, event.motion.yrel);
                }
            }
        }
        else
        {
            eventHandler.handleEvents(movement_controller, delta_time);
        }

        // --- Render ---
        renderer.render();

        // --- Physics ---
        shared_bullet_world->stepSimulation(1.0f / 60.f);

        // --- UI ---
        if (currentState == State::Connecting || currentState == State::Lobby ||
            currentState == State::Joining)
        {
            // Render text directly using the renderer's command buffer
            // UI elements are rendered by the renderer's post-scene pass
            // We just need to tell the renderer what to draw
        }

        // --- Gun animation (game only) ---
        if (currentState == State::Game)
        {
            glm::vec3 cam_pos = camera.getPosition();
            glm::vec3 velocity = (cam_pos - prevCamPos) / std::max(delta_time, 0.001f);
            prevCamPos = cam_pos;
            (void)velocity;

            if (firstPersonGun)
                firstPersonGun->addBob(delta_time, velocity);
        }

        // --- Pickup ---
        if (currentState == State::Game && !weaponPickedUp && dropCooldown == 0 && ready && client_api.clientInRoom())
        {
            glm::vec3 camPos = camera.getPosition();
            auto room_id = client_api.clientRoomId();
            auto game_items = client_api.getRemoteGameItemsSnapshot(room_id);

            for (auto& item : game_items)
            {
                if (item.status == "on_ground")
                {
                    float dist = glm::distance(camPos, glm::vec3(item.x, item.y, item.z));
                    if (dist < 100.0f)
                    {
                        weaponPickedUp = true;
                        pickedUpItemId = item.id;
                        pickedUpItemSize = glm::vec3(item.w, item.h, item.d);
                        pickedUpItemType = item.item_type;
                        pickedUpItemFilepath = item.filepath;

                        tcp_client.sendMessage(
                                packet::Room::GameItem::update(client_api, item.id, "picked_up"));

                        if (remote_game_items.find(item.id) != remote_game_items.end())
                        {
                            scene_manager.getCurrentScene()->removeObject(remote_game_items[item.id]->getId());
                            pendingDestroy[(frameCount - 1) % 3].push_back(std::move(remote_game_items[item.id]));
                            remote_game_items.erase(item.id);
                        }

                        firstPersonGun = std::make_shared<nx3d::client::gun::FirstPersonGun>(
                                device, physDevice,
                                ak47Texture, descriptorPool, descriptorSetLayout,
                                blt::ObjectData(shared_bullet_world, 0.0f,
                                                glm::vec3(0.0f, 0.0f, 0.0f), default_size));
                        scene_manager.getCurrentScene()->addObject(firstPersonGun);

                        crosshair = std::make_shared<nx3d::client::Crosshair>(
                                device, physDevice,
                                crosshairTexture, descriptorPool, descriptorSetLayout,
                                blt::ObjectData(shared_bullet_world, 0.0f,
                                                glm::vec3(0.0f, 0.0f, 0.0f), default_size));
                        uiRenderer.setOverlayCallback([&]()
                        {
                            if (crosshair)
                                crosshair->render(camera);
                        });

                        weaponHudText.setString("AK-47");
                        break;
                    }
                }
            }
        }

        // --- Network sync (game only) ---
        if (currentState == State::Game && ready && client_api.clientInRoom())
        {
            auto cam_pos = camera.getPosition();
            auto pos = nexilis::Vector3f({cam_pos.x, cam_pos.y, cam_pos.z});

            static nexilis::Vector3f lastSentPosition = {0.0f, 0.0f, 0.0f};
            constexpr float kPositionEpsilon = 0.001f;
            if (std::abs(pos.x - lastSentPosition.x) > kPositionEpsilon ||
                std::abs(pos.y - lastSentPosition.y) > kPositionEpsilon ||
                std::abs(pos.z - lastSentPosition.z) > kPositionEpsilon)
            {
                udp_client.sendMessage(packet::Room::Player3D::position(client_api, pos));
                lastSentPosition = pos;
            }

            auto room_id = client_api.clientRoomId();
            auto my_id = client_api.getClientId();

            auto players = client_api.getRemotePlayersSnapshot(room_id, my_id);

            for (auto& player : players)
            {
                if (remote_players.find(player.id) == remote_players.end())
                {
                    auto obj = std::make_shared<vk::TexturedCube>(
                            device, physDevice,
                            texture, descriptorPool, descriptorSetLayout,
                            blt::ObjectData(shared_bullet_world, 0.0f,
                                            glm::vec3(0.f, 0.f, 0.f), glm::vec3(10.0f, 10.0f, 10.0f)));
                    remote_players[player.id] = obj;
                    scene_manager.getCurrentScene()->addObject(obj);
                }
            }

            std::vector<uint64_t> to_remove;
            for (auto& [id, obj] : remote_players)
            {
                auto it = std::find_if(players.begin(), players.end(),
                                       [id](const auto& p)
                                       { return p.id == id; });
                if (it == players.end())
                {
                    scene_manager.getCurrentScene()->removeObject(obj->getId());
                    pendingDestroy[(frameCount - 1) % 3].push_back(std::move(obj));
                    to_remove.push_back(id);
                }
            }
            for (auto id : to_remove)
                remote_players.erase(id);

            for (auto& player : players)
            {
                auto it = remote_players.find(player.id);
                if (it != remote_players.end())
                {
                    it->second->setPosition(
                            {player.x, player.y, player.z});
                }
            }

            auto objects = client_api.getRemoteObjects3DSnapshot(room_id);

            for (auto& obj : objects)
            {
                if (remote_objects.find(obj.id) == remote_objects.end())
                {
                    auto cube = std::make_shared<vk::TexturedCube>(
                            device, physDevice,
                            niilo_texture, descriptorPool, descriptorSetLayout,
                            blt::ObjectData(shared_bullet_world, 0.0f,
                                            glm::vec3(0.f, 0.f, 0.f), default_size));
                    remote_objects[obj.id] = cube;
                    scene_manager.getCurrentScene()->addObject(cube);
                }
            }

            std::vector<uint64_t> objects_to_remove;
            for (auto& [id, cube] : remote_objects)
            {
                auto it = std::find_if(objects.begin(), objects.end(),
                                       [id](const auto& o)
                                       { return o.id == id; });
                if (it == objects.end())
                {
                    scene_manager.getCurrentScene()->removeObject(cube->getId());
                    pendingDestroy[(frameCount - 1) % 3].push_back(std::move(cube));
                    objects_to_remove.push_back(id);
                }
            }
            for (auto id : objects_to_remove)
                remote_objects.erase(id);

            for (auto& obj : objects)
            {
                auto it = remote_objects.find(obj.id);
                if (it != remote_objects.end())
                {
                    it->second->setPosition({obj.x, obj.y, obj.z});
                }
            }

            auto game_items = client_api.getRemoteGameItemsSnapshot(room_id);

            for (auto& item : game_items)
            {
                if (item.status == "on_ground")
                {
                    if (remote_game_items.find(item.id) == remote_game_items.end())
                    {
                        auto cube = std::make_shared<vk::TexturedCube>(
                                device, physDevice,
                                ak47Texture, descriptorPool, descriptorSetLayout,
                                blt::ObjectData(shared_bullet_world, 0.0f,
                                                glm::vec3(item.x, item.y, item.z),
                                                glm::vec3(item.w, item.h, item.d)));
                        remote_game_items[item.id] = cube;
                        scene_manager.getCurrentScene()->addObject(cube);
                    }
                }
                else
                {
                    if (remote_game_items.find(item.id) != remote_game_items.end())
                    {
                        scene_manager.getCurrentScene()->removeObject(remote_game_items[item.id]->getId());
                        pendingDestroy[(frameCount - 1) % 3].push_back(std::move(remote_game_items[item.id]));
                        remote_game_items.erase(item.id);
                    }
                }
            }

            std::vector<uint64_t> game_items_to_remove;
            for (auto& [id, cube] : remote_game_items)
            {
                auto it = std::find_if(game_items.begin(), game_items.end(),
                                       [id](const auto& i)
                                       { return i.id == id; });
                if (it == game_items.end())
                {
                    scene_manager.getCurrentScene()->removeObject(cube->getId());
                    pendingDestroy[(frameCount - 1) % 3].push_back(std::move(cube));
                    game_items_to_remove.push_back(id);
                }
            }
            for (auto id : game_items_to_remove)
                remote_game_items.erase(id);

            // --- Consume damage events ---
            auto damageEvents = client_api.consumeDamageEvents();
            for (auto& evt : damageEvents)
            {
                if (evt.target_id == my_id)
                {
                    health = static_cast<int>(evt.new_health);
                    if (health < 0)
                        health = 0;
                    healthText.setString("HP: " + std::to_string(health));
                }
                else if (evt.damage > 0.0f)
                {
                    hitmarkerFrames = 15;
                }
            }

            // --- Consume respawn events ---
            auto respawnEvents = client_api.consumeRespawnEvents();
            for (auto& evt : respawnEvents)
            {
                if (evt.target_id == my_id)
                {
                    auto& spawns = (chosenTeam == "Terrorist") ? tSpawns : ctSpawns;
                    auto spawn = spawns[rand() % spawns.size()];
                    camera.setPosition(spawn);
                    health = 100;
                    healthText.setString("HP: " + std::to_string(health));
                }
            }
        }

        window.update();
        time_interface.delay(16);
    }
}
