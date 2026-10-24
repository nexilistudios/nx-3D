#include <spear/event_handler.hh>
#include <spear/input.hh>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

int main()
{
    auto check = [](bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    };
    check(SDL_Init(SDL_INIT_EVENTS), "Could not initialize SDL events");

    spear::SDLInput input;
    input.sendKey(SDLK_W, true);
    input.sendMouseMotion(10.0f, 20.0f, 3.0f, -2.0f);
    input.sendMouseButton(SDL_BUTTON_LEFT, true, 10.0f, 20.0f);
    input.sendText(std::string(80, 'x'));

    SDL_Event event{};
    check(input.pollEvent(event) && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_W,
          "Key press was lost");
    check(input.pollEvent(event) && event.type == SDL_EVENT_MOUSE_MOTION && event.motion.xrel == 3.0f,
          "Mouse motion was lost");
    check(input.pollEvent(event) && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                  event.button.button == SDL_BUTTON_LEFT,
          "Mouse button press was lost");
    check(input.pollEvent(event) && event.type == SDL_EVENT_TEXT_INPUT &&
                  std::string(event.text.text) == std::string(80, 'x'),
          "Text input was lost");

    spear::Camera camera;
    spear::MovementController movement(camera);
    spear::EventHandler handler;
    bool sawRelease = false;
    handler.registerCallback(SDL_EVENT_MOUSE_BUTTON_UP, [&](const SDL_Event& mouse)
                             { sawRelease = mouse.button.button == SDL_BUTTON_LEFT; });
    input.sendKey(SDLK_W, true);
    handler.handleEvents(input, movement, 0.0f);
    check(handler.movementKeyDown(SDLK_W), "Pressed movement key was not tracked");

    input.sendMouseButton(SDL_BUTTON_LEFT, false);
    input.sendKey(SDLK_W, false);
    handler.handleEvents(input, movement, 0.0f);
    check(sawRelease, "Mouse release did not reach EventHandler");
    check(!handler.movementKeyDown(SDLK_W), "Released movement key is still held");

    SDL_Quit();
}
