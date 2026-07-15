#include <spear/spear_vulkan.hh>

#include <nexilis/client/packet.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/room_info.hh>
#include <nexilis/start_client.hh>
#include <nexilis/tcp_client.hh>

#include <btBulletDynamicsCommon.h>

#include <client/gun/first_person_gun.hh>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{

enum class State
{
    Connecting,
    Lobby,
    Joining,
    Game
};

} // namespace

int main()
{
    const std::string window_name = "nx_3D game";
    const spear::BaseWindow::Size window_size = {820, 640};
    std::atomic<bool> ready = false;
    std::mutex mtx;
    std::vector<nexilis::RoomInfo> rooms;

    using packet = nexilis::client::Packet;

    nexilis::ProtocolManager protocolManager;
    nexilis::TCPClient tcp_client(&protocolManager, "127.0.0.1", "password");

    auto start_client = nexilis::startClient(tcp_client, rooms, ready, mtx);
    start_client.detach();

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
    std::shared_ptr<nx3d::client::gun::FirstPersonGun> firstPersonGun;

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

    // HUD texts (used during Game state, top-left corner)
    spear::ui::vulkan::Text weaponHudText(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue(),
            descriptorPool, descriptorSetLayout, fontPath, 20);
    weaponHudText.setString("");
    weaponHudText.setColor(SDL_Color{0, 255, 0, 255});
    weaponHudText.setPosition(glm::vec2(-0.98f, -0.85f));

    // Menu list for rooms
    spear::ui::BaseMenuList* roomMenu = nullptr;

    renderer.setUIRenderer(&uiRenderer);

    uiRenderer.addExternalText(titleText);
    uiRenderer.addExternalText(statusText);
    uiRenderer.addExternalText(instructionsText);

    bool menuPopulated = false;
    std::atomic<bool> joiningRoom = false;
    uint64_t selectedRoomId = 0;
    State currentState = State::Connecting;

    // --- Event Handlers ---
    spear::EventHandler eventHandler;

    eventHandler.handleInput(SDLK_ESCAPE, [&device, &descriptorPool, &descriptorSetLayout]()
                             {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        exit(0); });

    eventHandler.handleInput(SDLK_P, [&ready, &tcp_client, &camera, &currentState]()
                             {
        if (currentState == State::Game && ready && tcp_client.getClientAPI().clientInRoom())
        {
            auto cam_pos = camera.getPosition();
            auto cam_front = camera.getFront();
            glm::vec3 spawn_pos = cam_pos + cam_front;
            auto pos = nexilis::Vector3f({spawn_pos.x, spawn_pos.y, spawn_pos.z});
            auto dim = nexilis::Vector3f({1.0f, 1.0f, 1.0f});
            tcp_client.sendMessage(
                    packet::Room::Object3D::create(pos, dim, ""));
        } });

    eventHandler.registerCallback(SDL_EVENT_QUIT, [&device, &descriptorPool, &descriptorSetLayout](const SDL_Event&)
                                  {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        exit(0); });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_MOTION, [&camera](const SDL_Event& event)
                                  { camera.rotate(event.motion.xrel, event.motion.yrel); });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_BUTTON_DOWN,
                                  [&currentState](const SDL_Event& event)
                                  {
                                      if (currentState == State::Game && event.button.button == SDL_BUTTON_LEFT)
                                          ;
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

    eventHandler.handleInput(SDLK_G, [&]() {
        if (currentState == State::Game && weaponPickedUp)
        {
            weaponPickedUp = false;

            scene_manager.getCurrentScene()->removeObject(firstPersonGun->getId());
            vkDeviceWaitIdle(device);
            firstPersonGun.reset();

            tcp_client.sendMessage(
                    packet::Room::GameItem::destroy(pickedUpItemId));

            glm::vec3 dropPos = camera.getPosition() + camera.getFront() * 300.0f;
            auto pos = nexilis::Vector3f({dropPos.x, dropPos.y, dropPos.z});
            auto dim = nexilis::Vector3f({pickedUpItemSize.x, pickedUpItemSize.y, pickedUpItemSize.z});
            tcp_client.sendMessage(
                    packet::Room::GameItem::create(pos, dim, pickedUpItemType, "on_ground", pickedUpItemFilepath));

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
        }

        if (currentState == State::Joining && tcp_client.getClientAPI().clientInRoom())
        {
            scene_manager.loadScene(game_scene_id);
            renderer.setScene(scene_manager.getCurrentScene());
            // Switch UI from lobby to HUD
            uiRenderer.clear();
            uiRenderer.addExternalText(weaponHudText);
            renderer.setUIRenderer(&uiRenderer);
            currentState = State::Game;
        }

        // --- Event handling ---
        if (currentState == State::Lobby || currentState == State::Connecting)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
                    exit(0);
                }
                if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    if (event.key.key == SDLK_ESCAPE)
                    {
                        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
                        exit(0);
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
                                            packet::Room::Management::join(selectedRoomId));
                                    joiningRoom = true;
                                    statusText.setString("Joining room...");
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
        if (currentState == State::Game && !weaponPickedUp && dropCooldown == 0 && ready && tcp_client.getClientAPI().clientInRoom())
        {
            glm::vec3 camPos = camera.getPosition();
            auto& api = tcp_client.getClientAPI();
            auto room_id = api.clientRoomId();
            auto game_items = api.getRemoteGameItemsSnapshot(room_id);

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
                                packet::Room::GameItem::update(item.id, "picked_up"));

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

                        weaponHudText.setString("AK-47");
                        break;
                    }
                }
            }
        }

        // --- Network sync (game only) ---
        if (currentState == State::Game && ready && tcp_client.getClientAPI().clientInRoom())
        {
            auto cam_pos = camera.getPosition();
            auto pos = nexilis::Vector3f({cam_pos.x, cam_pos.y, cam_pos.z});

            tcp_client.sendMessage(
                    packet::Room::Player3D::position(pos));

            auto& api = tcp_client.getClientAPI();
            auto room_id = api.clientRoomId();
            auto my_id = api.getClientId();

            auto players = api.getRemotePlayersSnapshot(room_id, my_id);

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

            auto objects = api.getRemoteObjects3DSnapshot(room_id);

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

            auto game_items = api.getRemoteGameItemsSnapshot(room_id);

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
        }

        window.update();
        time_interface.delay(16);
    }
}
