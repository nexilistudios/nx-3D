#include <client/app/client_app.hh>
#include <shared/ammunition.hh>

#include <client/chat/chat_text.hh>
#include <client/gamemode/bomb_mode.hh>
#include <client/gamemode/deathmatch_mode.hh>
#include <client/gamemode/weapon_profiles.hh>
#include <client/project_root.hh>

#include <nexilis/client/create_client_config.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/cmd_line_options.hh>
#include <nexilis/start_client.hh>
#include <nexilis/udp_client.hh>
#include <nexilis/util.hh>

#include <shared/fall_death.hh>
#include <shared/gamemode.hh>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace nx3d::client
{

namespace blt = spear::physics::bullet;
namespace vk = spear::rendering::vulkan;

ClientApp::ClientApp(const std::string& initialServerAddress, const std::string& initialUsername, bool connectOnStart)
    : window("nx_3D game", spear::BaseWindow::Size{820, 640}),
      camera(glm::vec3(-1600.0f, 64.0f, -2600.0f), glm::vec3(0.f, 1.f, 0.f), 90.0f, 0.f, 250.f),
      scene_manager(),
      bullet_world(),
      shared_world(std::make_shared<btDiscreteDynamicsWorld>(*bullet_world.getDynamicsWorld())),
      movement_controller(camera, shared_world.get(), 64.0f),
      renderer(window),
      serverAddress(initialServerAddress),
      username(initialUsername.empty() ? "username_" + std::to_string(nexilis::Util::getRandomInt(1000000, 9999999)) : initialUsername),
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

    descriptorPool = vk::Texture::createDescriptorPool(renderer.getDevice(), 512);
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
    smoke_texture = std::make_shared<vk::STBTexture>(
            renderer.getDevice(), renderer.getPhysicalDevice(),
            renderer.getCommandPool(), renderer.getGraphicsQueue());
    unsigned char smokePixel[4] = {137, 146, 150, 255};
    smoke_texture->loadFromRGBA(smokePixel, 1, 1);

    // --- Scenes ---
    setupScenes();
    scene_manager.loadScene(lobby_scene_id);

    // --- Audio ---
    audio_system.init();
    constexpr std::array<const char*, 6> shots = {
            "ak47", "m16", "glock", "usp", "deagle", "mp5"};
    for (std::size_t i = 0; i < shots.size(); ++i)
    {
        gunshot_audio[i] = std::make_unique<spear::audio::Sound>(
                audio_system, nx3d::projectAssetPath(std::string("sounds/") + shots[i] + ".wav"));
        gunshot_audio[i]->setSpatialRange(1200.0f, 2500.0f);
    }
    hitmarker_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/hitmarker.wav"));
    reload_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/reload.wav"));
    planting_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/planting.wav"));
    bomb_beep_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/bomb_beep.wav"));
    bomb_beep_audio->setSpatialRange(900.0f, 2500.0f);
    explosion_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/explosion.wav"));
    smoke_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/smoke.wav"));
    walk_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/walk.mp3"));
    step_audio = std::make_unique<spear::audio::Sound>(
            audio_system, nx3d::projectAssetPath("sounds/step.wav"));
    applyAudioVolume();

    // --- UI ---
    std::string fontPath = nx3d::projectAssetPath("fonts/firacode/static/FiraCode-Retina.ttf");
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
        usernameInput = username;
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
        } });

    // "y" opens the chat input line while playing.
    eventHandler.handleKeyPressed(SDLK_Y, [this]()
                                  {
        if (currentState == State::Game && !chatActive)
            openChat(); });

    eventHandler.handleKeyPressed(SDLK_B, [this]()
                                  {
        if (currentState == State::Game && !chatActive)
            openBuyMenu(); });

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
                                  {
                                      if (currentState == State::Game && game_mode && game_mode->canMove())
                                          camera.rotate(event.motion.xrel, event.motion.yrel); });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_BUTTON_DOWN,
                                  [this](const SDL_Event& event)
                                  {
                                      if (currentState == State::Game && game_mode)
                                          game_mode->handleMouseButtonDown(*this, event);
                                  });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_BUTTON_UP,
                                  [this](const SDL_Event& event)
                                  {
                                      if (currentState == State::Game && game_mode)
                                          game_mode->handleMouseButtonUp(*this, event);
                                  });

    eventHandler.registerCallback(SDL_EVENT_KEY_DOWN,
                                  [this](const SDL_Event& event)
                                  {
                                      if (currentState == State::Game && game_mode)
                                          game_mode->handleKeyDown(*this, event);
                                  });

    eventHandler.registerCallback(SDL_EVENT_KEY_UP,
                                  [this](const SDL_Event& event)
                                  {
                                      if (currentState == State::Game && game_mode)
                                          game_mode->handleKeyUp(*this, event);
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
    mapCollisionBody = meshRigidBody;

    auto game_objects = spear::Scene::Container{
            dust2_model};
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
    if (gameMode == nx3d::GameMode::bomb)
        return std::make_unique<gamemode::BombMode>();

    // Any gamemode without a client implementation yet falls back to
    // deathmatch so the room stays playable.
    return std::make_unique<gamemode::DeathmatchMode>();
}

void ClientApp::enterGame(const std::string& team)
{
    pose_send_elapsed = 1.0f;
    ui->showGameHudTexts();
    economyState = {};
    buyNotice.clear();
    buyErrorSeconds = 0.0f;
    ui->moneyText.setString("$800  |  B: Buy");
    ui->hideBuyMenu();
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
    applyAudioVolume();
    ui->updateVolumeDisplay(audioVolume);
}

void ClientApp::applyAudioVolume()
{
    for (auto& sound : gunshot_audio)
        if (sound) sound->setVolume(0.8f * std::sqrt(audioVolume));
    if (hitmarker_audio)
        hitmarker_audio->setVolume(audioVolume);
    if (reload_audio)
        reload_audio->setVolume(audioVolume);
    if (planting_audio)
        planting_audio->setVolume(audioVolume);
    if (bomb_beep_audio)
        bomb_beep_audio->setVolume(0.6f * std::sqrt(audioVolume));
    if (explosion_audio)
        explosion_audio->setVolume(std::min(1.0f, 2.5f * std::sqrt(audioVolume)));
    if (smoke_audio)
        smoke_audio->setVolume(0.65f * audioVolume);
    if (walk_audio)
        walk_audio->setVolume(audioVolume);
    if (step_audio)
        step_audio->setVolume(audioVolume);
}

spear::audio::Sound* ClientApp::gunshotSound(uint8_t sound)
{
    if (sound == audio::kSoundShoot) sound = audio::kSoundAk47;
    if (sound < audio::kSoundAk47 || sound > audio::kSoundMp5) return nullptr;
    return gunshot_audio[sound - audio::kSoundAk47].get();
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
        eventHandler.clearMovementKeys();
        quitHovered = false;
        ctTeamHovered = false;
        tTeamHovered = false;
        SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), false);
        ui->pauseTitle.setString("PAUSED");
        ui->pauseTeamTitle.setString("CHANGE TEAM");
        ui->pauseCTText.setString("COUNTER TERRORIST");
        ui->pauseTText.setString("TERRORIST");
        ui->pauseQuitText.setString("[ QUIT GAME ]");
        ui->pauseInstructions.setString("ESC: Resume | Click team | L/R: Volume | Enter: Quit");
        changeVolume(0.0f);
    }
}

void ClientApp::resumeGame()
{
    if (currentState == State::Paused)
    {
        currentState = State::Game;
        quitHovered = false;
        ctTeamHovered = false;
        tTeamHovered = false;
        SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
        ui->pauseTitle.setString("");
        ui->pauseTeamTitle.setString("");
        ui->pauseCTText.setString("");
        ui->pauseTText.setString("");
        ui->pauseVolumeText.setString("");
        ui->pauseQuitText.setString("");
        ui->pauseInstructions.setString("");
    }
}

void ClientApp::openBuyMenu()
{
    if (currentState != State::Game || !game_mode || !game_mode->canBuy())
        return;
    const std::string ownTeam = ui->teamDisplayText.getString();
    const auto pos = camera.getPosition();
    if (!nx3d::economy::inBuyZone(ownTeam, pos.x, pos.y, pos.z))
    {
        buyError = "Buy only in your team spawn";
        buyErrorSeconds = 2.0f;
        ui->matchStatusText.setString(buyError);
        return;
    }
    SDL_Event release{};
    release.type = SDL_EVENT_MOUSE_BUTTON_UP;
    release.button.button = SDL_BUTTON_LEFT;
    game_mode->handleMouseButtonUp(*this, release);
    currentState = State::Buy;
    buyNotice.clear();
    buyErrorSeconds = 0.0f;
    eventHandler.clearMovementKeys();
    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), false);
    ui->showBuyMenu(buyStyle, economyState, ownTeam, buyNotice,
                    selectedRoomGameMode == nx3d::GameMode::bomb);
}

void ClientApp::closeBuyMenu()
{
    if (currentState != State::Buy)
        return;
    currentState = State::Game;
    ui->hideBuyMenu();
    eventHandler.clearMovementKeys();
    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
}

void ClientApp::openChat()
{
    if (chatActive || currentState != State::Game)
        return;

    chatActive = true;
    chatInput.clear();
    lastChatDisplay.clear();
    ui->setChatInput("");
    SDL_StartTextInput(window.getSDLWindow());
    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), false);
    // Release any movement keys held when the chat opened, so the character
    // doesn't keep walking once keyboard control is handed back to the game.
    eventHandler.clearMovementKeys();
    // Keep the window up while the player types.
    chatVisibleTimer = std::max(chatVisibleTimer, kChatDisplaySeconds);
}

void ClientApp::closeChat(bool send)
{
    if (!chatActive)
        return;

    if (send && !chatInput.empty() && ready && client_api.clientInRoom())
    {
        // Broadcasts are relayed to everyone in the room, including the
        // sender, so our own message comes back and shows up (with our own
        // username) like everyone else's.
        tcp_client.sendMessage(
                nexilis::client::Packet::Room::Communicate::broadcast(client_api, chatInput));
    }

    chatActive = false;
    chatInput.clear();
    lastChatDisplay.clear();
    ui->setChatInput("");
    SDL_StopTextInput(window.getSDLWindow());
    SDL_SetWindowRelativeMouseMode(window.getSDLWindow(), true);
    eventHandler.clearMovementKeys();
}

void ClientApp::updateChatMessages()
{
    const auto room_id = client_api.clientRoomId();
    if (chatRoomId != room_id)
    {
        chatRoomId = room_id;
        chatMessagesConsumed = 0;
        chatMessages.clear();
    }

    // Snapshot the rooms under their mutex (same pattern as the nexilis
    // messaging example) and consume any messages we have not shown yet.
    for (const auto& room : client_api.getActiveRooms())
    {
        if (room.getId() != room_id)
            continue;

        const auto& messages = room.getMessages();
        const auto count = messages.size();
        for (; chatMessagesConsumed < count; ++chatMessagesConsumed)
        {
            const auto& message = messages[chatMessagesConsumed];
            const auto* sender = message.getClient();
            if (!sender && std::string_view(message.getPayload()).starts_with(nx3d::economy::kGrenadeMessagePrefix))
            {
                nx3d::economy::GrenadeThrow event;
                if (game_mode && nx3d::economy::decodeGrenade(message.getPayload(), event))
                    game_mode->onGrenadeThrown(*this, event);
                continue;
            }
            if (!sender && std::string_view(message.getPayload()).starts_with(nx3d::economy::kMessagePrefix))
            {
                nx3d::economy::Snapshot state;
                if (nx3d::economy::decode(message.getPayload(), state))
                {
                    economyState = std::move(state);
                    ui->moneyText.setString("$" + std::to_string(economyState.money) + "  |  B: Buy" +
                                           (economyState.hasDefuseKit ? "  |  KIT" : ""));
                    if (game_mode)
                        game_mode->onEconomySnapshot(*this, economyState);
                    buyNotice = economyState.notice;
                    if (currentState == State::Buy && game_mode)
                        ui->showBuyMenu(buyStyle, economyState,
                                        ui->teamDisplayText.getString(), buyNotice,
                                        selectedRoomGameMode == nx3d::GameMode::bomb);
                }
                continue;
            }
            const std::string name = sender ? sender->getUsername() : std::string("unknown");

            chatMessages.push_back(
                    {name, chat::truncate(message.getPayload(), chat::kMaxMessageCharacters)});
            if (static_cast<int>(chatMessages.size()) > kChatMaxLines)
                chatMessages.erase(chatMessages.begin());

            // A fresh message brings the window back up.
            chatVisibleTimer = kChatDisplaySeconds;
        }
        break;
    }
}

void ClientApp::updateChatWindow(float delta_time)
{
    if (!chatActive)
    {
        chatVisibleTimer -= delta_time;
        if (chatVisibleTimer < 0.0f)
            chatVisibleTimer = 0.0f;
    }

    const bool visible = chatActive || chatVisibleTimer > 0.0f;
    ui->setChatVisible(visible);

    if (!visible)
    {
        ui->clearChatRows();
        return;
    }

    // Wrap messages into visual lines, then retain the newest lines that fit.
    std::vector<std::string> lines;
    for (const auto& message : chatMessages)
    {
        auto wrapped = chat::wrapMessage(message.username, message.payload);
        lines.insert(lines.end(), wrapped.begin(), wrapped.end());
    }
    if (lines.size() > static_cast<std::size_t>(kChatMaxLines))
        lines.erase(lines.begin(), lines.end() - kChatMaxLines);

    for (int i = 0; i < kChatMaxLines; ++i)
    {
        if (i < static_cast<int>(lines.size()))
            ui->setChatLine(i, lines[i]);
        else
            ui->setChatLine(i, "");
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
        if (currentState == State::Connecting &&
            tcp_client.getProtocolStatus() == nexilis::client::ProtocolStatus::error)
            ui->statusText.setString("Connection failed. Check the server address and TCP port settings.");

        if (currentState == State::Connecting && ready && !menuPopulated)
        {
            tcp_client.sendMessage(packet::Set::General::username(client_api, username));

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
                    else if (event.key.key == SDLK_TAB)
                    {
                        activeTextInputField = (activeTextInputField + 1) % 2;
                    }
                    else if (event.key.key == SDLK_BACKSPACE)
                    {
                        if (activeTextInputField == 0 && !serverAddressInput.empty())
                        {
                            serverInputCleared = true;
                            serverAddressInput.pop_back();
                        }
                        else if (activeTextInputField == 1 && !usernameInput.empty())
                        {
                            usernameInputCleared = true;
                            usernameInput.pop_back();
                        }
                    }
                    else if (event.key.key == SDLK_RETURN)
                    {
                        std::string address = serverAddressInput.empty() ? "127.0.0.1" : serverAddressInput;
                        connect(address);
                        if (!usernameInput.empty())
                            username = usernameInput;
                        ui->showMenuTexts();
                        currentState = State::Connecting;
                    }
                }
                if (event.type == SDL_EVENT_TEXT_INPUT)
                {
                    if (activeTextInputField == 0)
                    {
                        if (!serverInputCleared)
                        {
                            serverAddressInput.clear();
                            serverInputCleared = true;
                        }
                        serverAddressInput += event.text.text;
                    }
                    else
                    {
                        if (!usernameInputCleared)
                        {
                            usernameInput.clear();
                            usernameInputCleared = true;
                        }
                        usernameInput += event.text.text;
                    }
                }
            }

            // Blinking cursor - only update when the displayed text changes
            // to avoid re-allocating a descriptor set on every frame.
            auto cursorText = [&](const std::string& text, bool activeField)
            {
                if (!activeField)
                    return text;
                return ((frameCount / 30) % 2 == 0) ? text + "|" : text + " ";
            };
            std::string serverDisplay = cursorText(serverAddressInput, activeTextInputField == 0);
            std::string usernameDisplay = cursorText(usernameInput, activeTextInputField == 1);
            if (serverDisplay != lastServerDisplay)
            {
                lastServerDisplay = serverDisplay;
                ui->serverAddressText.setString(serverDisplay);
            }
            if (usernameDisplay != lastUsernameDisplay)
            {
                lastUsernameDisplay = usernameDisplay;
                ui->usernameInputText.setString(usernameDisplay);
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
                    ctTeamHovered = (mx >= -0.44f && mx <= -0.02f && my >= 0.10f && my <= 0.23f);
                    tTeamHovered = (mx >= 0.02f && mx <= 0.44f && my >= 0.10f && my <= 0.23f);
                    if ((event.motion.state & SDL_BUTTON_LMASK) &&
                        mx >= -0.45f && mx <= 0.45f && my >= -0.20f && my <= -0.05f)
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
                    if (game_mode && mx >= -0.44f && mx <= -0.02f && my >= 0.10f && my <= 0.23f)
                    {
                        game_mode->changeTeam(*this, "Counter Terrorist");
                        resumeGame();
                        break;
                    }
                    if (game_mode && mx >= 0.02f && mx <= 0.44f && my >= 0.10f && my <= 0.23f)
                    {
                        game_mode->changeTeam(*this, "Terrorist");
                        resumeGame();
                        break;
                    }
                    if (mx >= -0.45f && mx <= 0.45f && my >= -0.20f && my <= -0.05f)
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
        else if (currentState == State::Buy)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                    cleanQuit();
                else if (event.type == SDL_EVENT_WINDOW_RESIZED)
                {
                    window.resize();
                    auto size = window.getSize();
                    renderer.setViewPort(size.x, size.y);
                }
                else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                {
                    if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_B)
                        closeBuyMenu();
                    else if (event.key.key == SDLK_TAB)
                    {
                        buyStyle = buyStyle == ui::Ui::BuyStyle::List ? ui::Ui::BuyStyle::Radial :
                                   buyStyle == ui::Ui::BuyStyle::Radial ? ui::Ui::BuyStyle::Cs2 :
                                                                           ui::Ui::BuyStyle::List;
                        ui->showBuyMenu(buyStyle, economyState,
                                        ui->teamDisplayText.getString(), buyNotice,
                                        selectedRoomGameMode == nx3d::GameMode::bomb);
                    }
                }
                else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                         event.button.button == SDL_BUTTON_LEFT)
                {
                    int width = 0, height = 0;
                    SDL_GetWindowSize(window.getSDLWindow(), &width, &height);
                    const float x = static_cast<float>(event.button.x) / static_cast<float>(width ? width : 1) * 2.0f - 1.0f;
                    const float y = 1.0f - static_cast<float>(event.button.y) / static_cast<float>(height ? height : 1) * 2.0f;
                    const int index = ui->buyItemAt(x, y);
                    if (index >= 0 && ready && client_api.clientInRoom())
                    {
                        const auto pos = camera.getPosition();
                        const auto forward = camera.getFront();
                        const float yaw = std::atan2(-forward.x, -forward.z);
                        tcp_client.sendMessage(packet::Room::Player3D::position(
                                client_api, nexilis::Vector3f({pos.x, pos.y, pos.z}), yaw));
                        tcp_client.sendMessage(packet::Room::Player3D::matchAction(
                                client_api, nx3d::economy::kItems[static_cast<std::size_t>(index)].action));
                    }
                }
            }
        }
        else if (currentState == State::Game && chatActive)
        {
            // Chat typing takes over the keyboard: events are polled manually
            // (like the menu states) so they don't feed player movement,
            // weapon handling or the camera through `eventHandler`.
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
                        closeChat(false);
                    }
                    else if (event.key.key == SDLK_RETURN)
                    {
                        closeChat(true);
                    }
                    else if (event.key.key == SDLK_BACKSPACE && !chatInput.empty())
                    {
                        chat::popBack(chatInput);
                    }
                }
                if (event.type == SDL_EVENT_TEXT_INPUT)
                {
                    chat::appendWithinLimit(chatInput, event.text.text);
                }
                if (event.type == SDL_EVENT_WINDOW_RESIZED)
                {
                    window.resize();
                    auto s = window.getSize();
                    renderer.setViewPort(s.x, s.y);
                }
            }
        }
        else
        {
            const bool canMove = currentState != State::Game ||
                                 (game_mode && game_mode->canMove());
            eventHandler.handleEvents(movement_controller, delta_time, canMove);
        }

        // Pausing stops player input, but an airborne player still needs
        // gravity and ground detection so they do not remain suspended until
        // the game is resumed.
        if ((currentState == State::Paused || currentState == State::Buy) &&
            !movement_controller.isOnGround())
        {
            static const std::unordered_map<SDL_Keycode, bool> noMovementKeys = {
                    {SDLK_W, false}, {SDLK_S, false}, {SDLK_A, false},
                    {SDLK_D, false}, {SDLK_SPACE, false}};
            movement_controller.processInput(noMovementKeys, delta_time);
        }

        // --- Render ---
        renderer.render();

        // --- Physics ---
        shared_world->stepSimulation(1.0f / 60.f);

        // --- Game update + network sync ---
        if (currentState == State::Game || currentState == State::Buy)
        {
            camera.updateRecoil(delta_time);

            // Keep the 3D audio listener glued to the camera so positional
            // sounds (other players' shots and footsteps) pan and attenuate
            // relative to where the player is looking.
            audio_system.setListener(
                    camera.getPosition(), camera.getFront(), camera.getWorldUp());

            if (game_mode)
            {
                game_mode->update(*this, delta_time);
                if (buyErrorSeconds > 0.0f)
                {
                    buyErrorSeconds = std::max(0.0f, buyErrorSeconds - delta_time);
                    ui->matchStatusText.setString(buyError);
                }
                if (currentState == State::Buy)
                {
                    const auto pos = camera.getPosition();
                    if (!game_mode->canBuy() ||
                        !nx3d::economy::inBuyZone(ui->teamDisplayText.getString(),
                                                  pos.x, pos.y, pos.z))
                        closeBuyMenu();
                }
                const std::string deathText = game_mode->isEliminated() ? "WASTED" : "";
                if (ui->wastedText.getString() != deathText)
                    ui->wastedText.setString(deathText);
            }

            if (ready && client_api.clientInRoom())
            {
                auto cam_pos = camera.getPosition();
                auto pos = nexilis::Vector3f({cam_pos.x, cam_pos.y, cam_pos.z});
                const glm::vec3 forward = camera.getFront();
                const float yaw = std::atan2(-forward.x, -forward.z);

                if (pos.y < nx3d::kFallDeathY)
                {
                    was_below_fall_plane = true;
                    // UDP movement may be lost. Repeat the out-of-bounds pose
                    // over TCP until the server has time to register the death.
                    fall_pose_send_elapsed += delta_time;
                    if (fall_pose_send_elapsed >= 0.5f)
                    {
                        tcp_client.sendMessage(packet::Room::Player3D::position(client_api, pos, yaw));
                        fall_pose_send_elapsed = 0.0f;
                    }
                }
                else
                {
                    if (was_below_fall_plane)
                    {
                        // Re-arm the server's fall check after respawn even
                        // if the first above-ground UDP update is dropped.
                        tcp_client.sendMessage(packet::Room::Player3D::position(client_api, pos, yaw));
                        was_below_fall_plane = false;
                    }
                    fall_pose_send_elapsed = 0.5f;
                }

                pose_send_elapsed += delta_time;
                constexpr float kPositionEpsilon = 0.001f;
                if (std::abs(pos.x - last_sent_position.x) > kPositionEpsilon ||
                    std::abs(pos.y - last_sent_position.y) > kPositionEpsilon ||
                    std::abs(pos.z - last_sent_position.z) > kPositionEpsilon ||
                    std::abs(std::remainder(yaw - last_sent_yaw, 6.2831853f)) > 0.01f ||
                    pose_send_elapsed >= 1.0f)
                {
                    udp_client.sendMessage(packet::Room::Player3D::position(client_api, pos, yaw));
                    last_sent_position = cam_pos;
                    last_sent_yaw = yaw;
                    pose_send_elapsed = 0.0f;
                }

                syncRemotePlayerTransforms(delta_time);
                syncRemoteObjects();
                syncRemoteGameItems();
                updateChatMessages();
            }

            // Chat window refresh: blinking cursor on the input line while
            // typing, plus the visibility countdown and row re-sync.
            if (chatActive)
            {
                std::string chatDisplay =
                        chat::tail(chatInput, chat::kLineCharacters - 1) +
                        (((frameCount / 30) % 2 == 0) ? "|" : " ");
                if (chatDisplay != lastChatDisplay)
                {
                    lastChatDisplay = chatDisplay;
                    ui->setChatInput(chatDisplay);
                }
            }
            updateChatWindow(delta_time);
        }

        window.update();
        time_interface.delay(16);
    }
}

void ClientApp::animatePlayerHit(uint64_t playerId)
{
    if (!remote_dead_players.contains(playerId))
        remote_player_animations[playerId].hitTime = 0.22f;
}

void ClientApp::animatePlayerDeath(uint64_t playerId)
{
    if (!remote_dead_players.insert(playerId).second)
        return;

    auto it = remote_players.find(playerId);
    if (it == remote_players.end())
        return;

    const auto& motion = remote_player_animations[playerId];
    if (auto* body = it->second->getRigidBody())
    {
        shared_world->removeRigidBody(body);
        it->second->setCollisionSize(btVector3(18.0f, 38.0f, 14.0f));
        btVector3 inertia(0.0f, 0.0f, 0.0f);
        body->getCollisionShape()->calculateLocalInertia(65.0f, inertia);
        body->setMassProps(65.0f, inertia);
        body->updateInertiaTensor();
        body->setCollisionFlags(body->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(btVector3(motion.position.x, motion.position.y - 26.0f,
                                      motion.position.z));
        transform.setRotation(btQuaternion(btVector3(0.0f, 1.0f, 0.0f), motion.facing));
        body->setWorldTransform(transform);
        body->getMotionState()->setWorldTransform(transform);
        body->setDamping(0.35f, 0.45f);
        body->setGravity(btVector3(0.0f, -450.0f, 0.0f));
        shared_world->addRigidBody(body);
        body->setLinearVelocity(btVector3(-std::sin(motion.facing) * 105.0f, 55.0f,
                                          -std::cos(motion.facing) * 105.0f));
        body->setAngularVelocity(btVector3(3.2f, 0.8f, 1.0f));
        body->activate();
    }
    death_animations.push_back({std::move(it->second), motion.position, motion.facing, 0.0f});
    remote_players.erase(it);
    remote_player_animations.erase(playerId);
}

void ClientApp::animatePlayerRespawn(uint64_t playerId)
{
    remote_dead_players.erase(playerId);
    remote_player_animations.erase(playerId);
}

void ClientApp::syncRemotePlayerTransforms(float deltaTime)
{
    const float dt = std::clamp(deltaTime, 0.0f, 0.05f);
    player_animation_clock += dt;

    for (auto it = death_animations.begin(); it != death_animations.end();)
    {
        it->elapsed += dt;
        if (it->elapsed >= 2.5f)
        {
            scene_manager.getCurrentScene()->removeObject(it->model->getId());
            pendingDestroy[(frameCount - 1) % 3].push_back(std::move(it->model));
            it = death_animations.erase(it);
            continue;
        }

        if (auto* body = it->model->getRigidBody())
        {
            btTransform transform;
            body->getMotionState()->getWorldTransform(transform);
            const auto& origin = transform.getOrigin();
            const auto& rotation = transform.getRotation();
            glm::mat4 pose = glm::translate(glm::mat4(1.0f),
                                            glm::vec3(origin.x(), origin.y(), origin.z()));
            pose *= glm::mat4_cast(glm::quat(rotation.w(), rotation.x(), rotation.y(), rotation.z()));
            pose = glm::translate(pose, glm::vec3(0.0f, 26.0f, 0.0f));
            it->model->setModel(pose);
        }
        ++it;
    }

    auto room_id = client_api.clientRoomId();
    auto my_id = client_api.getClientId();
    auto players = client_api.getRemotePlayersSnapshot(room_id, my_id);

    for (auto& player : players)
    {
        const std::string team = game_mode ? game_mode->playerTeam(player.id) : std::string{};
        // A player still choosing a team is not yet in the game. Create the
        // figure once the server has announced its team, then replace it if
        // the server swaps sides at halftime.
        if (team.empty())
            continue;
        if (remote_dead_players.contains(player.id))
            continue;
        auto existing = remote_players.find(player.id);
        if (existing != remote_players.end() && remote_player_teams[player.id] != team)
        {
            scene_manager.getCurrentScene()->removeObject(existing->second->getId());
            pendingDestroy[(frameCount - 1) % 3].push_back(std::move(existing->second));
            remote_players.erase(existing);
            remote_player_animations.erase(player.id);
        }
        if (remote_players.find(player.id) == remote_players.end())
        {
            const char* model = team == "Terrorist" ? "terrorist" : "counter_terrorist";
            const std::string asset = std::string("players/") + model;
            auto obj = std::make_shared<vk::OBJModel>(
                    renderer.getDevice(), renderer.getPhysicalDevice(),
                    renderer.getCommandPool(), renderer.getGraphicsQueue(),
                    nx3d::projectAssetPath(asset + ".obj"),
                    nx3d::projectAssetPath(asset + ".mtl"),
                    descriptorPool, descriptorSetLayout,
                    blt::ObjectData(shared_world, 0.0f,
                                    glm::vec3(player.x, player.y, player.z),
                                    default_size));
            remote_players[player.id] = obj;
            remote_player_teams[player.id] = team;
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
    {
        remote_players.erase(id);
        remote_player_teams.erase(id);
        remote_player_animations.erase(id);
    }

    for (auto& player : players)
    {
        auto it = remote_players.find(player.id);
        if (it != remote_players.end())
        {
            const glm::vec3 position(player.x, player.y, player.z);
            auto& motion = remote_player_animations[player.id];
            if (!motion.initialized)
            {
                motion.position = position;
                motion.initialized = true;
            }

            const glm::vec2 movement(position.x - motion.position.x,
                                     position.z - motion.position.z);
            const float distance = glm::length(movement);
            const float speed = dt > 0.0f && distance < 40.0f ? distance / dt : 0.0f;
            motion.facing = player.yaw;
            motion.speed += (std::clamp(speed / 250.0f, 0.0f, 1.0f) - motion.speed) *
                            std::min(1.0f, dt * 10.0f);
            motion.stride += motion.speed * dt * 7.0f;
            motion.hitTime = std::max(0.0f, motion.hitTime - dt);
            motion.position = position;

            const float bob = std::sin(motion.stride * 2.0f) * 0.9f * motion.speed +
                              std::sin(player_animation_clock * 2.5f) * 0.22f * (1.0f - motion.speed);
            glm::mat4 pose = glm::translate(glm::mat4(1.0f), position + glm::vec3(0.0f, bob, 0.0f));
            pose = glm::rotate(pose, motion.facing, glm::vec3(0.0f, 1.0f, 0.0f));
            pose = glm::rotate(pose, std::sin(motion.stride) * 0.045f * motion.speed,
                               glm::vec3(1.0f, 0.0f, 0.0f));
            pose = glm::rotate(pose, std::sin(motion.stride) * 0.035f * motion.speed + std::sin(motion.hitTime / 0.22f * 3.14159f) * 0.14f,
                               glm::vec3(0.0f, 0.0f, 1.0f));
            it->second->setModel(pose);
        }
    }

    for (auto it = remote_dead_players.begin(); it != remote_dead_players.end();)
    {
        if (std::none_of(players.begin(), players.end(),
                         [id = *it](const auto& player)
                         { return player.id == id; }))
            it = remote_dead_players.erase(it);
        else
            ++it;
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
        const std::string weaponType(nx3d::ammunition::pickupWeaponType(item.item_type));
        if (item.status == "on_ground" && gamemode::hasWeaponProfile(weaponType))
        {
            if (remote_game_items.find(item.id) == remote_game_items.end())
            {
                const auto& profile = gamemode::getWeaponProfile(weaponType);
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
                    body->setDamping(0.35f, 0.8f);
                    body->setSleepingThresholds(0.1f, 0.1f);
                    if (pending_weapon_throw &&
                        glm::distance(pending_weapon_throw->position,
                                      glm::vec3(item.x, item.y, item.z)) < 0.1f)
                    {
                        const glm::vec3 impulse = pending_weapon_throw->direction * 320.0f +
                                                  glm::vec3(0.0f, 75.0f, 0.0f);
                        body->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
                        pending_weapon_throw.reset();
                    }
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
