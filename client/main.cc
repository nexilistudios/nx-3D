#include <spear/spear.hh>

#include <nexilis/client/packet.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/tcp_client.hh>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>
#include <unordered_map>

int main()
{
    const std::string window_name = "nx_3D game";
    const spear::BaseWindow::Size window_size = {820, 640};

    using packet = nexilis::client::Packet;

    nexilis::ProtocolManager protocolManager;
    nexilis::TCPClient nexilisClient(&protocolManager, "127.0.0.1", "password");

    std::atomic<bool> nexilisReady = false;

    // clang-format off
    std::thread nexilisThread([&]()
    {
        nexilisClient.start();

        nexilisClient.sendMessage(packet::Get::General::clientId());
        while (!nexilisClient.getClientAPI().isInitialized())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        nexilisClient.sendMessage(packet::Get::Info::rooms());

        std::promise<void> roomsPromise;
        auto roomsFuture = roomsPromise.get_future();
        auto waitFn = nexilisClient.getClientAPI().waitUntilRoomsCreated(roomsPromise);
        waitFn();
        roomsFuture.wait();

        auto& rooms = nexilisClient.getClientAPI().getActiveRooms();
        if (!rooms.empty())
        {
            nexilisClient.sendMessage(
                packet::Room::Management::join(rooms.front().getId()));
        }

        nexilisReady = true;
    });
    // clang-format on
    nexilisThread.detach();

    spear::VulkanWindow window(window_name, window_size);
    auto w_size = window.getSize();
    std::cout << "Window size x: " << w_size.x << " y: " << w_size.y << std::endl;

    spear::Camera camera(glm::vec3(0.0f, 0.0f, 4.0f));
    spear::MovementController movement_controller(camera);
    spear::SceneManager scene_manager;

    namespace blt = spear::physics::bullet;
    namespace vk = spear::rendering::vulkan;

    blt::World bullet_world;
    auto shared_bullet_world = std::make_shared<btDiscreteDynamicsWorld>(*bullet_world.getDynamicsWorld());
    auto default_size = glm::vec3(1.0f, 1.0f, 1.0f);

    vk::Renderer renderer(window);
    renderer.init();
    renderer.setBackgroundColor(0.1f, 0.5f, 0.85f, 1.0f);
    renderer.setCamera(&camera);

    VkDevice device = renderer.getDevice();
    VkPhysicalDevice physDevice = renderer.getPhysicalDevice();

    // --- Descriptor pool + layout (owned here, lifetime matches the app) ---
    VkDescriptorPool descriptorPool = vk::Texture::createDescriptorPool(device, 128);
    VkDescriptorSetLayout descriptorSetLayout = vk::Texture::createDescriptorSetLayout(device);

    // Initialize the textured pipeline using the sampler layout.
    renderer.initializeTexturedPipeline(descriptorSetLayout);

    // --- Texture ---
    auto texture = std::make_shared<vk::STBTexture>(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue());
    texture->loadFromFile(spear::getAssetPath("wallnut.jpg"));

    auto niiloTexture = std::make_shared<vk::STBTexture>(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue());
    niiloTexture->loadFromFile(spear::getAssetPath("niilo.jpg"));

    // clang-format off
    auto scene_objects = spear::Scene::Container{
        std::make_shared<vk::OBJModel>(
            device, physDevice,
            "/cube_pets/Models/OBJ-format/animal-bunny.obj", "/cube_pets/Models/OBJ-format/animal-bunny.mtl",
            texture,
            descriptorPool, descriptorSetLayout,
            blt::ObjectData(shared_bullet_world, 0.0f,
            glm::vec3(0.0f, 0.0f, -7.0f), default_size)
        )
    };
    // clang-format on

    auto scene_function = [](spear::Scene::Container&) {};
    auto scene_id = spear::createScene(scene_objects, scene_function, scene_manager);
    scene_manager.loadScene(scene_id);
    spear::Time time_interface;

    // clang-format off
    spear::EventHandler eventHandler;

    eventHandler.handleInput(SDLK_ESCAPE, [&device, &descriptorPool, &descriptorSetLayout]()
    {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        exit(0);
    });

    eventHandler.handleInput(SDLK_P, [&nexilisReady, &nexilisClient, &camera]()
    {
        if (nexilisReady && nexilisClient.getClientAPI().clientInRoom())
        {
            auto cam_pos = camera.getPosition();
            auto cam_front = camera.getFront();
            glm::vec3 spawn_pos = cam_pos + cam_front;
            auto pos = nexilis::Vector3f({spawn_pos.x, spawn_pos.y, spawn_pos.z});
            auto dim = nexilis::Vector3f({1.0f, 1.0f, 1.0f});
            nexilisClient.sendMessage(
                    packet::Room::Object3D::create(pos, dim, ""));
        }
    });

    eventHandler.registerCallback(SDL_EVENT_QUIT, [&device, &descriptorPool, &descriptorSetLayout](const SDL_Event&)
    {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        exit(0);
    });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_MOTION, [&camera](const SDL_Event& event)
                                  { camera.rotate(event.motion.xrel, event.motion.yrel); });

    eventHandler.registerCallback(SDL_EVENT_WINDOW_RESIZED, [&window, &renderer](const SDL_Event&)
    {
        window.resize();
        auto s = window.getSize();
        renderer.setViewPort(s.x, s.y);
    });
    // clang-format on

    renderer.setScene(scene_manager.getCurrentScene());

    std::unordered_map<uint64_t, std::shared_ptr<vk::TexturedCube>> remote_players;
    std::unordered_map<uint64_t, std::shared_ptr<vk::TexturedCube>> remote_objects;

    while (true)
    {
        float delta_time = time_interface.getDeltaTime();
        time_interface.updateFromMain(delta_time);

        eventHandler.handleEvents(movement_controller, delta_time);

        renderer.render();

        bullet_world.stepSimulation(1.0f / 60.f);

        auto cam_pos = camera.getPosition();
        auto pos = nexilis::Vector3f({cam_pos.x, cam_pos.y, cam_pos.z});

        if (nexilisReady && nexilisClient.getClientAPI().clientInRoom())
        {
            nexilisClient.sendMessage(
                    packet::Room::Player3D::position(pos));

            auto& api = nexilisClient.getClientAPI();
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
                                            glm::vec3(0.f, 0.f, 0.f), default_size));
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
                            niiloTexture, descriptorPool, descriptorSetLayout,
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

        window.update();

        time_interface.delay(16);
    }
}
