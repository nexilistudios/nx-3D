#include <client/app/client_app.hh>

#include <client/gamemode/deathmatch_mode.hh>
#include <client/gamemode/weapon_profiles.hh>
#include <client/project_root.hh>

#include <nexilis/client/create_client_config.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/cmd_line_options.hh>
#include <nexilis/start_client.hh>
#include <nexilis/udp_client.hh>

#include <shared/gamemode.hh>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>

namespace nx3d::client
{

namespace blt = spear::physics::bullet;
namespace vk = spear::rendering::vulkan;

ClientApp::ClientApp(const std::string& initialServerAddress, bool connectOnStart)
    : window("nx_3D game", spear::BaseWindow::Size{820, 640}),
      camera(glm::vec3(-1600.0f, 64.0f, -2600.0f), glm::vec3(0.f, 1.f, 0.f), 90.0f, 0.f, 250.f),
      scene_manager(),
      bullet_world(),
      shared_world(std::make_shared<btDiscreteDynamicsWorld>(*bullet_world.getDynamicsWorld())),
      movement_controller(camera, shared_world.get(), 64.0f),
      renderer(window),
      serverAddress(initialServerAddress),
      client_api(nexilis::client::createClientConfig(serverAddress, "password")),
      tcp_client(client_api),
      udp_client(client_api)
{
    using packet = nexilis::client::Packet;

    auto w_size = window.getSize();
    std::cout << "Window size x: " << w_size.x << " y: " << w_size.y << std::endl;

    renderer.init();
    renderer.setBackgroundColor(0.1f, 0.1f, 0.2f, 1.0f);
    renderer.setCamera(&camera);

    descriptorPool = vk::Texture::createDescriptorPool(renderer.getDevice(), 256);
    descriptorSetLayout = vk::Texture::createDescriptorSetLayout(renderer.getDevice());

    renderer.initializeTexturedPipeline(descriptorSetLayout);
    renderer.initializeUIPipeline(descriptorSetLayout);

    // --- Textures ---
    wallnut_texture = std::make_shared<vk::STBTexture>(
            renderer.getDevice(), renderer.getPhysicalDevice(),
            renderer.getCommandPool(), renderer.getGraphicsQueue());
    wallnut_texture->loadFromFile(spear::getAssetPath("wallnut.jpg"));

    niilo_texture = std::make_shared<vk::STBTexture>(
            renderer.getDevice(), renderer.getPhysicalDevice(),
            renderer.getCommandPool(), renderer.getGraphicsQueue());
    niilo_texture->loadFromFile(spear::getAssetPath("niilo.jpg"));

    // White 1x1 texture for crosshair quads.
    crosshair_texture = std::make_shared<vk::STBTexture>(
            renderer.getDevice(), renderer.getPhysicalDevice(),
            renderer.getCommandPool(), renderer.getGraphicsQueue());
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    crosshair_texture->loadFromRGBA(whitePixel, 1, 1);

    // --- Scenes ---
    setupScenes();
    scene_manager.loadScene(lobby_scene_id);

    // --- Audio ---
    audio_system.init();
    gunshot_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/gunshot.wav"));

    // --- UI ---
    std::string fontPath = "/usr/share/fonts/TTF/FiraCode-Retina.ttf";
    ui = std::make_unique<ui::Ui>(
            renderer.getDevice(), renderer.getPhysicalDevice(),
            renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath);

    renderer.setUIRenderer(&ui->renderer());

    if (connectOnStart)
    {
        currentState = State::Connecting;
        ui->showMenuTexts();
        connect(serverAddress);
    }
    else
    {
        currentState = State::ServerInput;
        ui->showServerInputTexts();
        SDL_StartTextInput(window.getSDLWindow());
    }

    // --- Event handlers ---
    eventHandler.handleKeyPressed(SDLK_ESCAPE, [this]()
                                  {
        if (currentState == State::Game)
        {
            pauseGame();
        }
        else if (currentState == State::Paused)
        {
            resumeGame();
        }
    });

    eventHandler.handleInput(SDLK_P, [this]()
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

    eventHandler.registerCallback(SDL_EVENT_QUIT, [this](const SDL_Event&)
                                  { cleanQuit(); });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_MOTION, [this](const SDL_Event& event)
                                  { camera.rotate(event.motion.xrel, event.motion.yrel); });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_BUTTON_DOWN,
                                  [this](const SDL_Event& event)
                                  {
        if (currentState == State::Game && game_mode)
            game_mode->handleMouseButtonDown(*this, event);
    });

    eventHandler.registerCallback(SDL_EVENT_KEY_DOWN,
                                  [this](const SDL_Event& event)
                                  {
        if (currentState == State::Game && game_mode)
            game_mode->handleKeyDown(*this, event);
    });

    eventHandler.registerCallback(SDL_EVENT_WINDOW_RESIZED, [this](const SDL_Event&)
                                  {
        window.resize();
        auto s = window.getSize();
        renderer.setViewPort(s.x, s.y); });

    renderer.setScene(scene_manager.getCurrentScene());
}

ClientApp::~ClientApp() = default;

void ClientApp::setupScenes()
{
    auto lobby_objects = spear::Scene::Container{};
    auto lobby_function = [](spear::Scene::Container&) {};
    lobby_scene_id = spear::createScene(lobby_objects, lobby_function, scene_manager);
    scene_manager.getSceneById(lobby_scene_id)->setName("lobby");

    // Game scene: de_dust2 map
    auto dust2_model = std::make_shared<vk::OBJModel>(
            renderer.getDevice(), renderer.getPhysicalDevice(),
            renderer.getCommandPool(), renderer.getGraphicsQueue(),
            nx3d::projectAssetPath("de_dust2/source/de_dust2.obj"), nx3d::projectAssetPath("de_dust2/source/de_dust2.mtl"),
            descriptorPool, descriptorSetLayout,
            blt::ObjectData(shared_world, 0.0f,
                            glm::vec3(0.0f, 0.0f, 0.0f), default_size));
    // Rotate the map: de_dust2 OBJ uses Z as vertical (CS:GO convention),
    // but the engine uses Y as vertical (OpenGL convention).
    dust2_model->rotate(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // Build triangle mesh from OBJ for ground collision (leaked intentionally at exit).
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
    shared_world->addRigidBody(meshRigidBody);

    auto game_objects = spear::Scene::Container{
            dust2_model
    };
    auto game_function = [](spear::Scene::Container&) {};
    game_scene_id = spear::createScene(game_objects, game_function, scene_manager);
    scene_manager.getSceneById(game_scene_id)->setName("game");
}

void ClientApp::connect(const std::string& address)
{
    serverAddress = address;
    tcp_client.stop();
    client_api = nexilis::client::ClientAPI(
            nexilis::client::createClientConfig(serverAddress, "password"));
    tcp_client = protocol_manager.createProtocol<nexilis::TCPClient>(client_api);
    udp_client = protocol_manager.createProtocol<nexilis::UDPClient>(client_api);

    auto start_client = nexilis::startClient(client_api, tcp_client, rooms, ready, mtx);
    start_client.detach();
}

std::unique_ptr<gamemode::GameMode> ClientApp::makeGameMode(nx3d::GameMode gameMode)
{
    if (gameMode == nx3d::GameMode::deathmatch)
        return std::make_unique<gamemode::DeathmatchMode>();

    // Any gamemode without a client implementation yet falls back to
    // deathmatch so the room stays playable.
    return std::make_unique<gamemode::DeathmatchMode>();
}

void ClientApp::enterGame(const std::string& team)
{
    ui->showGameHudTexts();
    renderer.setUIRenderer(&ui->renderer());
    scene_manager.loadScene(game_scene_id);
    renderer.setScene(scene_manager.getCurrentScene());
    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
    currentState = State::Game;

    if (!game_mode)
        game_mode = makeGameMode(selectedRoomGameMode);
    game_mode->onEnter(*this, team);
}

void ClientApp::changeVolume(float delta)
{
    audioVolume = std::clamp(audioVolume + delta, 0.0f, 1.0f);
    if (gunshot_audio)
        gunshot_audio->setVolume(audioVolume);
    ui->updateVolumeDisplay(audioVolume);
}

void ClientApp::setVolumeNormalized(float normalized)
{
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    float snapped = std::round(normalized * 20.0f) / 20.0f;
    changeVolume(snapped - audioVolume);
}

void ClientApp::pauseGame()
{
    if (currentState == State::Game)
    {
        currentState = State::Paused;
        quitHovered = false;
        SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), false);
        ui->pauseTitle.setString("PAUSED");
        ui->pauseQuitText.setString("[ QUIT GAME ]");
        ui->pauseInstructions.setString("ESC: Resume   |   L/R: Volume   |   Enter or Click: Quit");
        changeVolume(0.0f);
    }
}

void ClientApp::resumeGame()
{
    if (currentState == State::Paused)
    {
        currentState = State::Game;
        SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
        ui->pauseTitle.setString("");
        ui->pauseVolumeText.setString("");
        ui->pauseQuitText.setString("");
        ui->pauseInstructions.setString("");
    }
}

void ClientApp::cleanQuit()
{
    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), false);
    if (client_api.clientInRoom())
    {
        tcp_client.sendMessage(nexilis::client::Packet::Room::Management::leave(client_api));
    }
    tcp_client.stop();
    vkDestroyDescriptorSetLayout(renderer.getDevice(), descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(renderer.getDevice(), descriptorPool, nullptr);
    exit(0);
}

void ClientApp::run()
{
    using packet = nexilis::client::Packet;

    while (true)
    {
        pendingDestroy[frameCount % 3].clear();
        frameCount++;

        float delta_time = time_interface.getDeltaTime();
        time_interface.updateFromMain(delta_time);

        // Recycle finished gunshot sounds.
        audio_system.update();

        // --- State machine ---
        if (currentState == State::Connecting && ready && !menuPopulated)
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (!rooms.empty())
            {
                roomMenu = &ui->createRoomMenu();
                roomMenu->setPosition(glm::vec2(-0.8f, 0.3f));
                roomMenu->setSpacing(40.0f);
                for (auto& room : rooms)
                    roomMenu->addItem(room.getName());
                ui->statusText.setString("Select a room and press Enter to join");
                ui->instructionsText.setString("Arrow keys: Navigate   |   Enter: Join   |   ESC: Quit");
            }
            else
            {
                ui->statusText.setString("No rooms available");
            }
            menuPopulated = true;
            currentState = State::Lobby;
            udp_client.start();
        }

        if (currentState == State::Joining && client_api.clientInRoom())
        {
            ui->showTeamSelectTexts();
            renderer.setUIRenderer(&ui->renderer());
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
                        std::string address = serverAddressInput.empty() ? "127.0.0.1" : serverAddressInput;
                        connect(address);
                        ui->showMenuTexts();
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
                ui->serverAddressText.setString(displayText);
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
                                    selectedRoomGameMode = gameModeFromName(rooms[idx].getName());
                                    tcp_client.sendMessage(
                                            packet::Room::Management::join(client_api, selectedRoomId));
                                    joiningRoom = true;
                                    ui->statusText.setString("Joining room...");
                                    currentState = State::Joining;
                                }
                            }
                        }
                    }
                    if (currentState == State::TeamSelect)
                    {
                        if (event.key.key == SDLK_1)
                        {
                            enterGame("Counter Terrorist");
                        }
                        else if (event.key.key == SDLK_2)
                        {
                            enterGame("Terrorist");
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
                            enterGame("Counter Terrorist");
                        }
                        else if (mx >= 0.15f && mx <= 0.8f && my >= 0.0f && my <= 0.15f)
                        {
                            enterGame("Terrorist");
                        }
                    }
                }
                if (event.type == SDL_EVENT_MOUSE_MOTION && currentState == State::Game)
                {
                    camera.rotate(event.motion.xrel, event.motion.yrel);
                }
            }
        }
        else if (currentState == State::Paused)
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
                        resumeGame();
                    }
                    else if (event.key.key == SDLK_LEFT)
                    {
                        changeVolume(-0.05f);
                    }
                    else if (event.key.key == SDLK_RIGHT)
                    {
                        changeVolume(0.05f);
                    }
                    else if (event.key.key == SDLK_RETURN)
                    {
                        cleanQuit();
                    }
                }
                if (event.type == SDL_EVENT_MOUSE_MOTION)
                {
                    int live_w = 0, live_h = 0;
                    SDL_GetWindowSize(window.getSDLWindow(), &live_w, &live_h);
                    float mx = (static_cast<float>(event.motion.x) / static_cast<float>(live_w ? live_w : 1)) * 2.0f - 1.0f;
                    float my = 1.0f - (static_cast<float>(event.motion.y) / static_cast<float>(live_h ? live_h : 1)) * 2.0f;
                    quitHovered = (mx >= -0.42f && mx <= 0.42f && my >= -0.37f && my <= -0.25f);
                    if ((event.motion.state & SDL_BUTTON_LMASK) &&
                        mx >= -0.45f && mx <= 0.45f && my >= -0.04f && my <= 0.11f)
                    {
                        setVolumeNormalized((mx + 0.45f) / 0.9f);
                    }
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
                {
                    int live_w = 0, live_h = 0;
                    SDL_GetWindowSize(window.getSDLWindow(), &live_w, &live_h);
                    float mx = (static_cast<float>(event.button.x) / static_cast<float>(live_w ? live_w : 1)) * 2.0f - 1.0f;
                    float my = 1.0f - (static_cast<float>(event.button.y) / static_cast<float>(live_h ? live_h : 1)) * 2.0f;
                    if (mx >= -0.45f && mx <= 0.45f && my >= -0.04f && my <= 0.11f)
                    {
                        setVolumeNormalized((mx + 0.45f) / 0.9f);
                    }
                    else if (mx >= -0.42f && mx <= 0.42f && my >= -0.37f && my <= -0.25f)
                    {
                        cleanQuit();
                    }
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
        shared_world->stepSimulation(1.0f / 60.f);

        // --- Game update + network sync ---
        if (currentState == State::Game)
        {
            camera.updateRecoil(delta_time);

            if (game_mode)
                game_mode->update(*this, delta_time);

            if (ready && client_api.clientInRoom())
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

                syncRemotePlayerTransforms();
                syncRemoteObjects();
                syncRemoteGameItems();
            }
        }

        window.update();
        time_interface.delay(16);
    }
}

void ClientApp::syncRemotePlayerTransforms()
{
    auto room_id = client_api.clientRoomId();
    auto my_id = client_api.getClientId();
    auto players = client_api.getRemotePlayersSnapshot(room_id, my_id);

    for (auto& player : players)
    {
        if (remote_players.find(player.id) == remote_players.end())
        {
            auto obj = std::make_shared<vk::TexturedCube>(
                    renderer.getDevice(), renderer.getPhysicalDevice(),
                    wallnut_texture, descriptorPool, descriptorSetLayout,
                    blt::ObjectData(shared_world, 0.0f,
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
}

void ClientApp::syncRemoteObjects()
{
    auto room_id = client_api.clientRoomId();
    auto objects = client_api.getRemoteObjects3DSnapshot(room_id);

    for (auto& obj : objects)
    {
        if (remote_objects.find(obj.id) == remote_objects.end())
        {
            auto cube = std::make_shared<vk::TexturedCube>(
                    renderer.getDevice(), renderer.getPhysicalDevice(),
                    niilo_texture, descriptorPool, descriptorSetLayout,
                    blt::ObjectData(shared_world, 0.0f,
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
}

void ClientApp::syncRemoteGameItems()
{
    auto room_id = client_api.clientRoomId();
    auto game_items = client_api.getRemoteGameItemsSnapshot(room_id);

    for (auto& item : game_items)
    {
        if (item.status == "on_ground")
        {
            if (remote_game_items.find(item.id) == remote_game_items.end())
            {
                const auto& profile = gamemode::getWeaponProfile(item.item_type);
                auto obj = std::make_shared<vk::OBJModel>(
                        renderer.getDevice(), renderer.getPhysicalDevice(),
                        renderer.getCommandPool(), renderer.getGraphicsQueue(),
                        profile.obj_path, profile.mtl_path,
                        descriptorPool, descriptorSetLayout,
                        blt::ObjectData(shared_world, 5.0f,
                                        glm::vec3(item.x, item.y, item.z),
                                        default_size),
                        false);
                obj->setCollisionSize(profile.half_extents);
                if (btRigidBody* body = obj->getRigidBody())
                {
                    body->setDamping(0.9f, 1.0f);
                    body->setSleepingThresholds(0.1f, 0.1f);
                }
                obj->rotate(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                obj->scale(glm::vec3(profile.ground_scale));
                remote_game_items[item.id] = obj;
                scene_manager.getCurrentScene()->addObject(obj);
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

    // Sync the physics-driven position of each on-ground item back to
    // its visual transform (guns fall under gravity and settle).
    for (auto& item : game_items)
    {
        auto it = remote_game_items.find(item.id);
        if (it != remote_game_items.end())
        {
            btVector3 p = it->second->getPosition();
            it->second->setPosition(glm::vec3(p.x(), p.y(), p.z()));
        }
    }
}

} // namespace nx3d::client