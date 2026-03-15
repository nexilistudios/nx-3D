#include <spear/spear.hh>

#include <iostream>

int main()
{
    const std::string window_name = "Spear application-vulkan";
    const spear::BaseWindow::Size window_size = {820, 640};

    spear::VulkanWindow window(window_name, window_size);
    auto w_size = window.getSize();
    std::cout << "Window size x: " << w_size.x << " y: " << w_size.y << std::endl;

    spear::Camera camera(glm::vec3(0.0f, 0.0f, 4.0f));
    spear::MovementController movement_controller(camera);
    spear::SceneManager scene_manager;

    namespace bullet = spear::physics::bullet;
    namespace vulkan = spear::rendering::vulkan;

    bullet::World bullet_world;
    auto shared_bullet_world = std::make_shared<btDiscreteDynamicsWorld>(*bullet_world.getDynamicsWorld());
    auto default_size = glm::vec3(1.0f, 1.0f, 1.0f);

    vulkan::Renderer renderer(window);
    renderer.init();
    renderer.setBackgroundColor(0.1f, 0.1f, 0.15f, 1.0f);
    renderer.setCamera(&camera);

    VkDevice device = renderer.getDevice();
    VkPhysicalDevice physDevice = renderer.getPhysicalDevice();

    // --- Descriptor pool + layout (owned here, lifetime matches the app) ---
    VkDescriptorPool descriptorPool = vulkan::Texture::createDescriptorPool(device, 8);
    VkDescriptorSetLayout descriptorSetLayout = vulkan::Texture::createDescriptorSetLayout(device);

    // Initialize the textured pipeline using the sampler layout.
    renderer.initializeTexturedPipeline(descriptorSetLayout);

    // --- Texture ---
    auto texture = std::make_shared<vulkan::STBTexture>(
            device, physDevice, renderer.getCommandPool(), renderer.getGraphicsQueue());
    texture->loadFromFile(spear::getAssetPath("wallnut.jpg"));

    // clang-format off
    auto scene_objects = spear::Scene::Container{
        std::make_shared<vulkan::TexturedCube>(
            device, physDevice,
            texture,
            descriptorPool, descriptorSetLayout,
            bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(1.5f, 0.0f, 0.0f), default_size)),
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

    while (true)
    {
        float delta_time = time_interface.getDeltaTime();
        time_interface.updateFromMain(delta_time);

        eventHandler.handleEvents(movement_controller, delta_time);

        renderer.setScene(scene_manager.getCurrentScene());
        renderer.render();

        bullet_world.stepSimulation(1.0f / 60.f);

        window.update();

        time_interface.delay(16);
    }
}
