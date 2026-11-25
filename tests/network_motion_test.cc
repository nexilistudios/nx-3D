#include <client/app/network_motion.hh>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main()
{
    try
    {
        for (int fps : {30, 60, 144})
        {
            glm::vec3 rendered(0.0f);
            for (int frame = 1; frame <= fps; ++frame)
            {
                const float x = 280.0f * frame / fps;
                rendered = nx3d::client::smoothRemotePosition(
                        rendered, {x, 0.0f, 0.0f}, 1.0f / fps);
                require(rendered.x <= x && x - rendered.x < 12.0f,
                        "Remote walk did not follow smoothly at this frame rate");
            }
            require(rendered.x > 268.0f, "Remote walk lagged too far behind");
            for (int frame = 0; frame < fps / 4; ++frame)
                rendered = nx3d::client::smoothRemotePosition(
                        rendered, {280.0f, 0.0f, 0.0f}, 1.0f / fps);
            require(glm::distance(rendered, glm::vec3(280.0f, 0.0f, 0.0f)) < 0.02f,
                    "Remote player did not settle after stopping");
        }

        glm::vec3 sparse(0.0f);
        float largestStep = 0.0f;
        for (int frame = 1; frame <= 120; ++frame)
        {
            const float x = 28.0f * (frame / 6);
            const auto next = nx3d::client::smoothRemotePosition(
                    sparse, {x, 0.0f, 0.0f}, 1.0f / 60.0f);
            largestStep = std::max(largestStep, next.x - sparse.x);
            sparse = next;
        }
        require(largestStep < 12.0f,
                "Sparse position updates still made remote players jump");

        const glm::vec3 teleported(400.0f, 64.0f, -200.0f);
        require(nx3d::client::smoothRemotePosition({}, teleported, 1.0f / 60.0f) == teleported,
                "Teleport was smoothed across the map");
        const float facing = nx3d::client::smoothRemoteFacing(3.1f, -3.1f, 1.0f / 60.0f);
        require(facing > 3.1f && facing < 3.2f, "Facing crossed the long way around");
        const glm::vec3 predicted(20.0f, 64.0f, 0.0f);
        const glm::vec3 server(10.0f, 64.0f, 0.0f);
        require(nx3d::client::reconcileLocalPosition(predicted, server,
                                                     1.0f / 60.0f, false) == predicted,
                "Stale server position pulled back local movement");
        require(nx3d::client::reconcileLocalPosition(teleported, server,
                                                     1.0f / 60.0f, false) == teleported,
                "A long update gap snapped local movement backward");
        const auto corrected = nx3d::client::reconcileLocalPosition(
                predicted, server, 1.0f / 60.0f, true);
        require(corrected.x < predicted.x && corrected.x > server.x,
                "Fresh server position did not make a bounded correction");
        require(nx3d::client::reconcileLocalPosition(predicted, teleported,
                                                     1.0f / 60.0f, true, true) == teleported,
                "Large local correction did not snap to the server");
        std::cout << "Network movement checks passed at 30, 60 and 144 FPS\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
