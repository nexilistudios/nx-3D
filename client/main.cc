#include <spear/camera.hh>
#include <spear/create_scene.hh>
#include <spear/event_handler.hh>
#include <spear/model/obj_loader.hh>
#include <spear/movement_controller.hh>
#include <spear/scene_manager.hh>
#include <spear/time.hh>

#include <spear/window/opengl_window.hh>

#include <spear/rendering/opengl/model/obj_model.hh>
#include <spear/rendering/opengl/renderer.hh>
#include <spear/rendering/opengl/shader.hh>
#include <spear/rendering/opengl/shapes/cube.hh>
#include <spear/rendering/opengl/shapes/quad.hh>
#include <spear/rendering/opengl/shapes/sphere.hh>
#include <spear/rendering/opengl/texture/sdl_texture.hh>

#include <spear/physics/bullet/world.hh>

#include <iostream>

// TODO
// #include <spear/sprite_3d.hh>

int main()
{
    const std::string window_name = "Spear application";
    const spear::BaseWindow::Size window_size = {820, 640};
    const spear::rendering::API gl_api = spear::rendering::API::OpenGL;

    spear::OpenGLWindow window(window_name, window_size);
    window.hideCursor(true);
    window.setMouseGrab(true);
    window.setRelativeMouseMode(true);

    auto w_size = window.getSize();
    std::cout << "Window size x: " << w_size.x << " y: " << w_size.y << std::endl;

    spear::Camera camera;
    spear::MovementController movement_controller(camera);
    spear::SceneManager scene_manager;

    namespace bullet = spear::physics::bullet;
    namespace opengl = spear::rendering::opengl;

    opengl::Renderer renderer(window);
    renderer.init();
    renderer.setViewPort(w_size.x, w_size.y);
    renderer.setBackgroundColor(0.2f, 0.3f, 0.4f, 1.0f);

    // Bullet world.
    bullet::World bullet_world;
    auto shared_bullet_world = std::make_shared<btDiscreteDynamicsWorld>(*bullet_world.getDynamicsWorld());

    // Texture creation.
    auto niilo_texture = std::make_shared<opengl::SDLTexture>("niilo.jpg");
    auto wallnut_texture = std::make_shared<opengl::SDLTexture>("wallnut.jpg");

    auto default_size = glm::vec3(1.0f, 1.0f, 1.0f);

    // clang-format off
    auto scene1_objects = spear::Scene::Container {
        std::make_shared<opengl::Cube>(wallnut_texture, bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f), default_size)),
        std::make_shared<opengl::OBJModel>("test.obj", "test.mtl", wallnut_texture,
                                   bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(1.0f, 5.0f, -15.0f), default_size)),
        opengl::Cube::create(niilo_texture, bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(1.0f, 5.0f, 13.0f), default_size)),
        std::make_shared<opengl::Quad>(bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(2.0f, 8.0f, 1.0f), default_size), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f)),
        std::make_shared<opengl::Sphere>(niilo_texture, bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(4.0f, 3.f, -4.f), default_size))
    };
    auto scene1_function = [](spear::Scene::Container& objects)
    {
        objects[0]->translate(glm::vec3(0.0f, -4.0f, 0.0f));
        objects[0]->scale(glm::vec3(1000.0f, 1.0f, 1000.0f));
    };

    auto scene2_objects = spear::Scene::Container {
        std::make_shared<opengl::Cube>(wallnut_texture, bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(10.0f, 2.0f, 101.0f), default_size)),
        std::make_shared<opengl::Quad>(bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(122.0f, -228.0f, -231.0f), default_size), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f)),
        std::make_shared<opengl::Sphere>(niilo_texture, bullet::ObjectData(shared_bullet_world, 1.0f, glm::vec3(204.0f, 343.f, -4324.4f), default_size))
    };
    auto scene2_function = [](spear::Scene::Container& objects)
    {
        objects[0]->translate(glm::vec3(0.0f, -4.0f, 0.0f));
    };

    auto scene1_id = spear::createScene(scene1_objects, scene1_function, scene_manager);
    auto scene2_id = spear::createScene(scene2_objects, scene1_function, scene_manager);

    // Scene swapping.
    spear::Time time_interface;
    time_interface.addEvent(1.f, 2.f, [&scene_manager, scene2_id]()
    {
        scene_manager.loadScene(scene2_id);
        std::cout << "Running scene 2: " << scene2_id << std::endl;
    });
    time_interface.addEvent(2.f, 3.f, [&scene_manager, scene1_id]()
    {
        scene_manager.loadScene(scene1_id);
        std::cout << "Running scene 1: " << scene1_id << std::endl;
    });

    spear::EventHandler eventHandler;

    eventHandler.handleInput(SDLK_ESCAPE, [](){
        exit(0);
    });

    eventHandler.registerCallback(SDL_EVENT_MOUSE_BUTTON_DOWN, [](const SDL_Event& event)
    {
        std::cout << "Mouse button pressed at (" << event.button.x << ", " << event.button.y << ")" << std::endl;
    });

    eventHandler.registerCallback(SDL_EVENT_QUIT, [](const SDL_Event&)
    {
        std::cout << "Quit event received. Exiting..." << std::endl;
        exit(0);
    });

    // Mouse movement.
    eventHandler.registerCallback(SDL_EVENT_MOUSE_MOTION, [&camera](const SDL_Event& event)
                                  { camera.rotate(event.motion.xrel, event.motion.yrel); });

    // Update window size.
    eventHandler.registerCallback(SDL_EVENT_WINDOW_RESIZED, [&window, &renderer](const SDL_Event&)
    {
        std::cout << "Window resized!" << std::endl;
        window.resize();
        auto w_size = window.getSize();
        renderer.setViewPort(w_size.x, w_size.y);
    });
    // clang-format on

    // Set scene1 as starting scene.
    scene_manager.loadScene(scene1_id);
    auto current_scene = scene_manager.getCurrentScene();

    while (true)
    {
        // Update scenes.
        auto* new_scene = scene_manager.getCurrentScene();
        if (current_scene != new_scene)
        {
            current_scene = new_scene;
        }

        float delta_time = time_interface.getDeltaTime();
        time_interface.updateFromMain(delta_time);

        // Rendering.
        renderer.render();

        // Update object's in scene.
        current_scene->update(camera);

        // Run bullet simulation.
        bullet_world.stepSimulation(1.0f / 60.f);

        // Event handling.
        eventHandler.handleEvents(movement_controller, delta_time);

        // Update SDL_Window if using OpenGL.
        window.update();

        time_interface.delay(16); // 60 fps.
    }
}
