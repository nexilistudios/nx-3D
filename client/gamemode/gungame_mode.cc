#include <client/gamemode/gungame_mode.hh>

#include <client/app/client_app.hh>

namespace nx3d::client::gamemode
{
void GunGameMode::onGunGameResult(ClientApp& app, const nx3d::gungame::Result& result)
{
    m_result = result;
    m_triggerHeld = false;
    app.ui->showGunGameResults(result);
}

void GunGameMode::update(ClientApp& app, float dt)
{
    DeathmatchMode::update(app, dt);
    if (m_result)
    {
        if (app.ui->moneyText.getString() != "GUN GAME FINISHED")
            app.ui->moneyText.setString("GUN GAME FINISHED");
        app.ui->showGunGameResults(*m_result);
        return;
    }
    if (m_health > 0 && !m_stageWeapon.empty() && m_pickedUpItemType != m_stageWeapon &&
        m_primary.type == m_stageWeapon)
        equipSlot(app, 1, true);
    int stage = 1;
    for (std::size_t i = 0; i < nx3d::gungame::kWeapons.size(); ++i)
        if (nx3d::gungame::kWeapons[i] == m_stageWeapon)
            stage = static_cast<int>(i) + 1;
    const auto status = "GUN GAME  |  Stage " + std::to_string(stage) + "/" +
                        std::to_string(nx3d::gungame::kWeapons.size()) +
                        "  |  Kill to advance  |  Knife kill wins";
    if (app.ui->moneyText.getString() != status)
        app.ui->moneyText.setString(status);
}
} // namespace nx3d::client::gamemode
