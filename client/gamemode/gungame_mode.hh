#ifndef NX3D_CLIENT_GAMEMODE_GUNGAME_MODE_HH
#define NX3D_CLIENT_GAMEMODE_GUNGAME_MODE_HH

#include <client/gamemode/deathmatch_mode.hh>
#include <shared/gungame.hh>

#include <algorithm>
#include <optional>

namespace nx3d::client::gamemode
{
class GunGameMode : public DeathmatchMode
{
public:
    nx3d::GameMode type() const override
    {
        return nx3d::GameMode::gungame;
    }
    bool canBuy() const override
    {
        return false;
    }
    bool canMove() const override
    {
        return !m_result && m_restartSeconds < 0 && DeathmatchMode::canMove();
    }

    void handleKeyDown(ClientApp& app, const SDL_Event& event) override
    {
        if (m_result || m_restartSeconds >= 0 || event.key.key == SDLK_G || event.key.key == SDLK_F)
            return;
        if (event.key.key >= SDLK_1 && event.key.key <= SDLK_3 &&
            event.key.key != (m_stageWeapon == "knife" ? SDLK_3 : SDLK_1))
            return;
        DeathmatchMode::handleKeyDown(app, event);
    }
    void handleKeyUp(ClientApp& app, const SDL_Event& event) override
    {
        if (!m_result && m_restartSeconds < 0)
            DeathmatchMode::handleKeyUp(app, event);
    }
    void handleMouseButtonDown(ClientApp& app, const SDL_Event& event) override
    {
        if (!m_result && m_restartSeconds < 0)
            DeathmatchMode::handleMouseButtonDown(app, event);
    }

    void onEconomySnapshot(ClientApp& app, const nx3d::economy::Snapshot& state) override
    {
        if (state.effect == "weapon")
            m_stageWeapon = state.weapon;
        DeathmatchMode::onEconomySnapshot(app, state);
    }

    void onGunGameResult(ClientApp& app, const nx3d::gungame::Result& result) override;
    void onGunGameCountdown(ClientApp& app, int seconds) override;

    void update(ClientApp& app, float dt) override;

protected:
    bool preserveEquipmentOnRespawn() const override
    {
        return true;
    }

private:
    std::string m_stageWeapon;
    std::optional<nx3d::gungame::Result> m_result;
    int m_restartSeconds = -1;
};
} // namespace nx3d::client::gamemode

#endif
