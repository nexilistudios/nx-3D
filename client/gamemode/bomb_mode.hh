#ifndef NX3D_CLIENT_GAMEMODE_BOMB_MODE_HH
#define NX3D_CLIENT_GAMEMODE_BOMB_MODE_HH

#include <client/gamemode/deathmatch_mode.hh>

namespace nx3d::client::gamemode
{
class BombMode : public DeathmatchMode
{
public:
    nx3d::GameMode type() const override
    {
        return nx3d::GameMode::bomb;
    }
    void onEnter(ClientApp& app, const std::string& team) override;
    void update(ClientApp& app, float deltaTime) override;
    void handleMouseButtonDown(ClientApp& app, const SDL_Event& event) override;
    void handleKeyDown(ClientApp& app, const SDL_Event& event) override;
    void handleKeyUp(ClientApp& app, const SDL_Event& event) override;

private:
    bool m_active = false;
    bool m_alive = false;
    bool m_planted = false;
    bool m_using = false;
    float m_seconds = 0.0f;
    uint64_t m_round = 0;
    uint64_t m_tScore = 0;
    uint64_t m_ctScore = 0;
    uint64_t m_queuePosition = 0;
    std::string m_phase = "waiting";
    std::string m_notice;
    std::shared_ptr<spear::rendering::vulkan::TexturedCube> m_bombCube;
};
} // namespace nx3d::client::gamemode
#endif
