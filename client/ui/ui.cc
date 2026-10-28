#include <client/ui/ui.hh>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <unordered_set>

namespace nx3d::client::ui
{

Ui::Ui(VkDevice device,
       VkPhysicalDevice physDevice,
       VkCommandPool commandPool,
       VkQueue graphicsQueue,
       VkDescriptorPool descriptorPool,
       VkDescriptorSetLayout descriptorSetLayout,
       const std::string& fontPath)
    : m_device(device),
      m_physDevice(physDevice),
      m_commandPool(commandPool),
      m_graphicsQueue(graphicsQueue),
      m_descriptorPool(descriptorPool),
      m_descriptorSetLayout(descriptorSetLayout),
      m_fontPath(fontPath),
      m_renderer(device, physDevice, commandPool, graphicsQueue,
                 descriptorPool, descriptorSetLayout, fontPath, 24),

      titleText(device, physDevice, commandPool, graphicsQueue,
                descriptorPool, descriptorSetLayout, fontPath, 32),
      statusText(device, physDevice, commandPool, graphicsQueue,
                 descriptorPool, descriptorSetLayout, fontPath, 20),
      instructionsText(device, physDevice, commandPool, graphicsQueue,
                       descriptorPool, descriptorSetLayout, fontPath, 16),
      serverPlayerCountText(device, physDevice, commandPool, graphicsQueue,
                            descriptorPool, descriptorSetLayout, fontPath, 18),

      serverInputLabel(device, physDevice, commandPool, graphicsQueue,
                       descriptorPool, descriptorSetLayout, fontPath, 24),
      serverAddressText(device, physDevice, commandPool, graphicsQueue,
                        descriptorPool, descriptorSetLayout, fontPath, 24),
      serverInputInstructions(device, physDevice, commandPool, graphicsQueue,
                              descriptorPool, descriptorSetLayout, fontPath, 16),

      usernameInputLabel(device, physDevice, commandPool, graphicsQueue,
                         descriptorPool, descriptorSetLayout, fontPath, 24),
      usernameInputText(device, physDevice, commandPool, graphicsQueue,
                        descriptorPool, descriptorSetLayout, fontPath, 24),

      ctButtonText(device, physDevice, commandPool, graphicsQueue,
                   descriptorPool, descriptorSetLayout, fontPath, 24),
      tButtonText(device, physDevice, commandPool, graphicsQueue,
                  descriptorPool, descriptorSetLayout, fontPath, 24),
      teamSelectTitle(device, physDevice, commandPool, graphicsQueue,
                      descriptorPool, descriptorSetLayout, fontPath, 32),
      teamSelectInstructions(device, physDevice, commandPool, graphicsQueue,
                             descriptorPool, descriptorSetLayout, fontPath, 16),
      healthText(device, physDevice, commandPool, graphicsQueue,
                 descriptorPool, descriptorSetLayout, fontPath, 24),
      weaponHudText(device, physDevice, commandPool, graphicsQueue,
                    descriptorPool, descriptorSetLayout, fontPath, 20),
      hitmarkerText(device, physDevice, commandPool, graphicsQueue,
                    descriptorPool, descriptorSetLayout, fontPath, 28),
      wastedText(device, physDevice, commandPool, graphicsQueue,
                 descriptorPool, descriptorSetLayout, fontPath, 64),
      teamDisplayText(device, physDevice, commandPool, graphicsQueue,
                      descriptorPool, descriptorSetLayout, fontPath, 20),
      roundText(device, physDevice, commandPool, graphicsQueue,
                descriptorPool, descriptorSetLayout, fontPath, 22),
      matchStatusText(device, physDevice, commandPool, graphicsQueue,
                      descriptorPool, descriptorSetLayout, fontPath, 17),
      bombText(device, physDevice, commandPool, graphicsQueue,
               descriptorPool, descriptorSetLayout, fontPath, 20),
      moneyText(device, physDevice, commandPool, graphicsQueue,
                descriptorPool, descriptorSetLayout, fontPath, 20),
      fpsText(device, physDevice, commandPool, graphicsQueue,
              descriptorPool, descriptorSetLayout, fontPath, 16),
      buyTitleText(device, physDevice, commandPool, graphicsQueue,
                   descriptorPool, descriptorSetLayout, fontPath, 30),
      buyHintText(device, physDevice, commandPool, graphicsQueue,
                  descriptorPool, descriptorSetLayout, fontPath, 16),
      buyNoticeText(device, physDevice, commandPool, graphicsQueue,
                    descriptorPool, descriptorSetLayout, fontPath, 18),

      pauseTitle(device, physDevice, commandPool, graphicsQueue,
                 descriptorPool, descriptorSetLayout, fontPath, 48),
      pauseTeamTitle(device, physDevice, commandPool, graphicsQueue,
                     descriptorPool, descriptorSetLayout, fontPath, 18),
      pauseCTText(device, physDevice, commandPool, graphicsQueue,
                  descriptorPool, descriptorSetLayout, fontPath, 16),
      pauseTText(device, physDevice, commandPool, graphicsQueue,
                 descriptorPool, descriptorSetLayout, fontPath, 16),
      pauseQuitText(device, physDevice, commandPool, graphicsQueue,
                    descriptorPool, descriptorSetLayout, fontPath, 28),
      pauseSettingsText(device, physDevice, commandPool, graphicsQueue,
                        descriptorPool, descriptorSetLayout, fontPath, 26),
      pauseInstructions(device, physDevice, commandPool, graphicsQueue,
                        descriptorPool, descriptorSetLayout, fontPath, 14),
      settingsTitle(device, physDevice, commandPool, graphicsQueue,
                    descriptorPool, descriptorSetLayout, fontPath, 38),
      settingsHint(device, physDevice, commandPool, graphicsQueue,
                   descriptorPool, descriptorSetLayout, fontPath, 14),
      settingsBack(device, physDevice, commandPool, graphicsQueue,
                   descriptorPool, descriptorSetLayout, fontPath, 20),

      leaderboardTitle(device, physDevice, commandPool, graphicsQueue,
                       descriptorPool, descriptorSetLayout, fontPath, 28),
      leaderboardTHeader(device, physDevice, commandPool, graphicsQueue,
                         descriptorPool, descriptorSetLayout, fontPath, 22),
      leaderboardCTHeader(device, physDevice, commandPool, graphicsQueue,
                          descriptorPool, descriptorSetLayout, fontPath, 22),

      chatTitle(device, physDevice, commandPool, graphicsQueue,
                descriptorPool, descriptorSetLayout, fontPath, 14),
      chatInputText(device, physDevice, commandPool, graphicsQueue,
                    descriptorPool, descriptorSetLayout, fontPath, 18)
{
    // --- Leaderboard (Tab) ----------------------------------------------------
    leaderboardTitle.setString("");
    leaderboardTitle.setColor(SDL_Color{255, 255, 255, 255});
    leaderboardTitle.setPosition(glm::vec2(-0.12f, 0.44f));

    leaderboardTHeader.setString("");
    leaderboardTHeader.setColor(SDL_Color{255, 150, 0, 255});
    leaderboardTHeader.setPosition(glm::vec2(-0.8f, 0.34f));

    leaderboardCTHeader.setString("");
    leaderboardCTHeader.setColor(SDL_Color{0, 150, 255, 255});
    leaderboardCTHeader.setPosition(glm::vec2(0.0f, 0.34f));

    serverPlayerCountText.setString("");
    serverPlayerCountText.setColor(SDL_Color{205, 228, 240, 255});
    serverPlayerCountText.setPosition(glm::vec2(0.68f, 0.93f));

    teamRosterRows.reserve(kTeamRosterMaxRows);
    for (int i = 0; i < kTeamRosterMaxRows; ++i)
    {
        auto row = std::make_unique<spear::ui::vulkan::Text>(
                device, physDevice, commandPool, graphicsQueue,
                descriptorPool, descriptorSetLayout, fontPath, 14);
        row->setString("");
        teamRosterRows.push_back(std::move(row));
    }

    buildLeaderboard();
    buildChat();
    buildBuyMenu();
    // --- Lobby / connecting ----------------------------------------------
    titleText.setString("nx-3D Lobby");
    titleText.setColor(SDL_Color{0, 200, 255, 255});
    titleText.setPosition(glm::vec2(-0.8f, 0.7f));

    statusText.setString("Connecting to server...");
    statusText.setColor(SDL_Color{200, 200, 200, 255});
    statusText.setPosition(glm::vec2(-0.8f, 0.5f));

    instructionsText.setString("");
    instructionsText.setPosition(glm::vec2(-0.8f, -0.8f));

    // --- Server address input ---------------------------------------------
    serverInputLabel.setString("Server address:");
    serverInputLabel.setColor(SDL_Color{255, 255, 255, 255});
    serverInputLabel.setPosition(glm::vec2(-0.8f, 0.5f));

    serverAddressText.setString("|");
    serverAddressText.setColor(SDL_Color{0, 200, 255, 255});
    serverAddressText.setPosition(glm::vec2(-0.8f, 0.3f));

    // --- Username input ----------------------------------------------------
    usernameInputLabel.setString("Username:");
    usernameInputLabel.setColor(SDL_Color{255, 255, 255, 255});
    usernameInputLabel.setPosition(glm::vec2(-0.8f, 0.1f));

    usernameInputText.setString("|");
    usernameInputText.setColor(SDL_Color{0, 200, 255, 255});
    usernameInputText.setPosition(glm::vec2(-0.8f, -0.1f));

    serverInputInstructions.setString("Type and press Enter   |   Tab: Switch field   |   ESC: Quit");
    serverInputInstructions.setColor(SDL_Color{200, 200, 200, 255});
    serverInputInstructions.setPosition(glm::vec2(-0.8f, -0.8f));

    // --- Team select --------------------------------------------------------
    ctButtonText.setString("Counter Terrorist");
    ctButtonText.setColor(SDL_Color{255, 255, 255, 255});
    ctButtonText.setPosition(glm::vec2(-0.72f, 0.03f));

    tButtonText.setString("Terrorist");
    tButtonText.setColor(SDL_Color{255, 255, 255, 255});
    tButtonText.setPosition(glm::vec2(0.35f, 0.03f));

    teamSelectTitle.setString("Choose Your Team");
    teamSelectTitle.setColor(SDL_Color{255, 255, 255, 255});
    teamSelectTitle.setPosition(glm::vec2(-0.3f, 0.4f));

    teamSelectInstructions.setString("Click a team to join");
    teamSelectInstructions.setColor(SDL_Color{200, 200, 200, 255});
    teamSelectInstructions.setPosition(glm::vec2(-0.25f, 0.25f));

    // --- In-game HUD ---------------------------------------------------------
    healthText.setString("HP: 100");
    healthText.setColor(SDL_Color{255, 255, 255, 255});
    healthText.setPosition(glm::vec2(-0.98f, -0.98f));

    weaponHudText.setString("");
    weaponHudText.setColor(SDL_Color{0, 255, 0, 255});
    weaponHudText.setPosition(glm::vec2(-0.98f, -0.88f));

    hitmarkerText.setString("");
    hitmarkerText.setColor(SDL_Color{255, 255, 255, 255});
    hitmarkerText.setPosition(glm::vec2(-0.025f, -0.025f));
    hitmarkerText.setScale(0.003f);

    wastedText.setString("");
    wastedText.setColor(SDL_Color{255, 70, 70, 255});
    wastedText.setPosition(glm::vec2(-0.27f, 0.0f));

    teamDisplayText.setString("");
    teamDisplayText.setColor(SDL_Color{255, 255, 255, 255});
    teamDisplayText.setPosition(glm::vec2(0.65f, -0.95f));
    roundText.setString("");
    roundText.setPosition(glm::vec2(-0.32f, 0.88f));
    roundText.setColor(SDL_Color{255, 255, 255, 255});
    matchStatusText.setString("");
    matchStatusText.setPosition(glm::vec2(-0.52f, 0.78f));
    matchStatusText.setColor(SDL_Color{255, 220, 140, 255});
    bombText.setString("");
    bombText.setPosition(glm::vec2(0.58f, 0.82f));
    bombText.setColor(SDL_Color{255, 100, 60, 255});
    moneyText.setString("$800  |  B: Buy");
    moneyText.setPosition(glm::vec2(0.56f, 0.69f));
    moneyText.setColor(SDL_Color{130, 255, 160, 255});
    fpsText.setString("");
    fpsText.setPosition(glm::vec2(-0.98f, 0.94f));
    fpsText.setColor(SDL_Color{190, 225, 190, 255});
    buyTitleText.setColor(SDL_Color{255, 255, 255, 255});
    buyTitleText.setPosition(glm::vec2(-0.40f, 0.65f));
    buyHintText.setColor(SDL_Color{200, 220, 230, 255});
    buyHintText.setPosition(glm::vec2(-0.68f, -0.68f));
    buyNoticeText.setColor(SDL_Color{255, 215, 120, 255});
    buyNoticeText.setPosition(glm::vec2(-0.48f, -0.55f));

    // --- Pause menu ----------------------------------------------------------
    pauseTitle.setString("");
    pauseTitle.setColor(SDL_Color{255, 255, 255, 255});
    pauseTitle.setPosition(glm::vec2(-0.17f, 0.47f));

    pauseTeamTitle.setString("");
    pauseTeamTitle.setColor(SDL_Color{200, 200, 200, 255});
    pauseTeamTitle.setPosition(glm::vec2(-0.15f, 0.30f));

    pauseCTText.setString("");
    pauseCTText.setColor(SDL_Color{210, 235, 255, 255});
    pauseCTText.setPosition(glm::vec2(-0.39f, 0.15f));

    pauseTText.setString("");
    pauseTText.setColor(SDL_Color{255, 220, 180, 255});
    pauseTText.setPosition(glm::vec2(0.12f, 0.15f));

    pauseQuitText.setString("");
    pauseQuitText.setColor(SDL_Color{255, 255, 255, 255});
    pauseQuitText.setPosition(glm::vec2(-0.22f, -0.33f));

    pauseSettingsText.setString("");
    pauseSettingsText.setColor(SDL_Color{255, 255, 255, 255});
    pauseSettingsText.setPosition(glm::vec2(-0.22f, -0.13f));

    pauseInstructions.setString("");
    pauseInstructions.setColor(SDL_Color{200, 200, 200, 255});
    pauseInstructions.setPosition(glm::vec2(-0.37f, -0.56f));

    settingsTitle.setString("");
    settingsTitle.setColor(SDL_Color{255, 255, 255, 255});
    settingsTitle.setPosition(glm::vec2(-0.23f, 0.64f));
    settingsHint.setString("");
    settingsHint.setColor(SDL_Color{195, 205, 220, 255});
    settingsHint.setPosition(glm::vec2(-0.62f, -0.71f));
    settingsBack.setString("");
    settingsBack.setColor(SDL_Color{255, 210, 130, 255});
    settingsBack.setPosition(glm::vec2(-0.17f, -0.59f));
    for (int i = 0; i < kCrosshairRowCount; ++i)
    {
        auto makeText = [&]
        {
            return std::make_unique<spear::ui::vulkan::Text>(
                device, physDevice, commandPool, graphicsQueue,
                descriptorPool, descriptorSetLayout, fontPath, 19);
        };
        const float y = 0.48f - i * 0.10f;
        auto addText = [&](auto& texts, float x, SDL_Color color)
        {
            auto text = makeText();
            text->setColor(color);
            text->setPosition(glm::vec2(x, y));
            texts.push_back(std::move(text));
        };
        addText(settingsRows, -0.60f, SDL_Color{220, 230, 240, 255});
        addText(settingsValues, 0.13f, SDL_Color{220, 230, 240, 255});
        addText(settingsLeftArrows, -0.06f, SDL_Color{255, 210, 130, 255});
        addText(settingsRightArrows, 0.55f, SDL_Color{255, 210, 130, 255});
    }

    for (int i = 0; i < 5; ++i)
    {
        auto text = std::make_unique<spear::ui::vulkan::Text>(
            device, physDevice, commandPool, graphicsQueue,
            descriptorPool, descriptorSetLayout, fontPath, 14);
        text->setColor(SDL_Color{195, 205, 220, 255});
        m_crosshairPreviewTexts.push_back(std::move(text));
    }
    constexpr std::array<float, 5> previewTextY{{0.64f, 0.53f, -0.28f, -0.74f, -0.83f}};
    for (int i = 0; i < 5; ++i)
        m_crosshairPreviewTexts[i]->setPosition(glm::vec2(0.29f, previewTextY[i]));

    buildPauseMenu();
    buildDeathOverlay();
}

void Ui::buildDeathOverlay()
{
    auto makeSolidTexture = [&](unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        auto texture = std::make_shared<spear::rendering::vulkan::STBTexture>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue);
        unsigned char pixel[4] = {r, g, b, a};
        texture->loadFromRGBA(pixel, 1, 1);
        return texture;
    };

    m_deathBackdropQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(45, 0, 0, 220));
    m_deathBackdropQuad->setPosition(glm::vec2(-1.0f, -1.0f));
    m_deathBackdropQuad->setSize(glm::vec2(2.0f, 2.0f));

    m_deathBandQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(0, 0, 0, 230));
    m_deathBandQuad->setPosition(glm::vec2(-1.0f, -0.16f));
    m_deathBandQuad->setSize(glm::vec2(2.0f, 0.32f));
}

void Ui::renderDeathOverlay(spear::ui::RenderContext ctx)
{
    if (m_deathBackdropQuad)
        m_deathBackdropQuad->render(ctx);
    if (m_deathBandQuad)
        m_deathBandQuad->render(ctx);
}

void Ui::buildPauseMenu()
{
    auto makeSolidTexture = [&](unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        auto texture = std::make_shared<spear::rendering::vulkan::STBTexture>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue);
        unsigned char pixel[4] = {r, g, b, a};
        texture->loadFromRGBA(pixel, 1, 1);
        return texture;
    };

    m_pauseBackdropQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(12, 12, 18, 215));
    m_pauseBackdropQuad->setPosition(glm::vec2(-1.0f, -1.0f));
    m_pauseBackdropQuad->setSize(glm::vec2(2.0f, 2.0f));

    m_pausePanelBorder = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(160, 160, 170, 255));
    m_pausePanelBorder->setPosition(glm::vec2(-0.52f, -0.62f));
    m_pausePanelBorder->setSize(glm::vec2(1.04f, 1.36f));

    m_pausePanel = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(26, 28, 38, 255));
    m_pausePanel->setPosition(glm::vec2(-0.50f, -0.60f));
    m_pausePanel->setSize(glm::vec2(1.0f, 1.32f));

    m_pauseCTButton = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(35, 85, 135, 255));
    m_pauseCTButton->setPosition(glm::vec2(-0.44f, 0.10f));
    m_pauseCTButton->setSize(glm::vec2(0.42f, 0.13f));

    m_pauseCTButtonHover = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(55, 135, 205, 255));
    m_pauseCTButtonHover->setPosition(glm::vec2(-0.44f, 0.10f));
    m_pauseCTButtonHover->setSize(glm::vec2(0.42f, 0.13f));

    m_pauseTButton = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(135, 75, 25, 255));
    m_pauseTButton->setPosition(glm::vec2(0.02f, 0.10f));
    m_pauseTButton->setSize(glm::vec2(0.42f, 0.13f));

    m_pauseTButtonHover = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(210, 120, 35, 255));
    m_pauseTButtonHover->setPosition(glm::vec2(0.02f, 0.10f));
    m_pauseTButtonHover->setSize(glm::vec2(0.42f, 0.13f));

    m_pauseQuitButton = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(140, 40, 40, 255));
    m_pauseQuitButton->setPosition(glm::vec2(-0.42f, -0.37f));
    m_pauseQuitButton->setSize(glm::vec2(0.84f, 0.12f));

    m_pauseQuitButtonHover = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(210, 80, 60, 255));
    m_pauseQuitButtonHover->setPosition(glm::vec2(-0.42f, -0.37f));
    m_pauseQuitButtonHover->setSize(glm::vec2(0.84f, 0.12f));

    m_pauseSettingsButton = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(42, 82, 104, 255));
    m_pauseSettingsButton->setPosition(glm::vec2(-0.42f, -0.17f));
    m_pauseSettingsButton->setSize(glm::vec2(0.84f, 0.12f));
    m_pauseSettingsButtonHover = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(66, 132, 165, 255));
    m_pauseSettingsButtonHover->setPosition(glm::vec2(-0.42f, -0.17f));
    m_pauseSettingsButtonHover->setSize(glm::vec2(0.84f, 0.12f));
    m_settingsPanel = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(26, 34, 48, 250));
    m_settingsPanel->setPosition(glm::vec2(-0.72f, -0.80f));
    m_settingsPanel->setSize(glm::vec2(1.44f, 1.60f));
    m_settingsSelection = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(46, 73, 96, 255));
    m_settingsSelection->setSize(glm::vec2(1.30f, 0.108f));
    auto makeQuad = [&](SDL_Color color, glm::vec2 position, glm::vec2 size)
    {
        auto quad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(color.r, color.g, color.b, color.a));
        quad->setPosition(position);
        quad->setSize(size);
        return quad;
    };
    constexpr std::array<SDL_Color, 3> backgrounds{{
        {153, 130, 94, 255}, {20, 24, 30, 255}, {218, 222, 227, 255}}};
    for (int i = 0; i < 3; ++i)
        m_crosshairPreviewPanels[i] = makeQuad(backgrounds[i], {0.26f, -0.68f}, {0.64f, 1.20f});
    for (int row : {2, 3, 4, 8, 9, 10, 11, 12})
    {
        const float y = 0.59f - row * 0.087f - 0.012f;
        m_crosshairSliderTracks[row] = makeQuad({65, 79, 95, 255}, {-0.27f, y}, {0.40f, 0.009f});
        const SDL_Color color = row == 9 ? SDL_Color{240, 100, 100, 255} :
            row == 10 ? SDL_Color{100, 220, 130, 255} :
            row == 11 ? SDL_Color{100, 165, 245, 255} : SDL_Color{255, 210, 130, 255};
        m_crosshairSliderFills[row] = makeQuad(color, {-0.27f, y}, {0.20f, 0.009f});
    }

}

void Ui::buildLeaderboard()
{
    auto makeSolidTexture = [&](unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        auto texture = std::make_shared<spear::rendering::vulkan::STBTexture>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue);
        unsigned char pixel[4] = {r, g, b, a};
        texture->loadFromRGBA(pixel, 1, 1);
        return texture;
    };

    m_leaderboardBackdropQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(0, 0, 0, 180));
    m_leaderboardBackdropQuad->setPosition(glm::vec2(-1.0f, -1.0f));
    m_leaderboardBackdropQuad->setSize(glm::vec2(2.0f, 2.0f));

    m_leaderboardPanelQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(18, 20, 30, 220));
    m_leaderboardPanelQuad->setPosition(glm::vec2(-0.90f, -0.12f));
    m_leaderboardPanelQuad->setSize(glm::vec2(1.80f, 0.68f));

    constexpr int maxRows = kLeaderboardMaxRows;
    const float startY = 0.24f;
    const float rowH = 0.055f;

    leaderboardTRows.resize(maxRows);
    leaderboardCTRows.resize(maxRows);
    for (int i = 0; i < maxRows; ++i)
    {
        leaderboardTRows[i] = std::make_unique<spear::ui::vulkan::Text>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue,
                m_descriptorPool, m_descriptorSetLayout, m_fontPath, 20);
        leaderboardTRows[i]->setColor(SDL_Color{255, 150, 0, 255});
        leaderboardTRows[i]->setPosition(glm::vec2(-0.80f, startY - i * rowH));
        leaderboardTRows[i]->setString("");

        leaderboardCTRows[i] = std::make_unique<spear::ui::vulkan::Text>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue,
                m_descriptorPool, m_descriptorSetLayout, m_fontPath, 20);
        leaderboardCTRows[i]->setColor(SDL_Color{0, 150, 255, 255});
        leaderboardCTRows[i]->setPosition(glm::vec2(0.0f, startY - i * rowH));
        leaderboardCTRows[i]->setString("");
    }
}

void Ui::buildChat()
{
    auto makeSolidTexture = [&](unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        auto texture = std::make_shared<spear::rendering::vulkan::STBTexture>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue);
        unsigned char pixel[4] = {r, g, b, a};
        texture->loadFromRGBA(pixel, 1, 1);
        return texture;
    };

    // Bottom-left chat window: x in [-0.98, -0.30], y in [-0.85, -0.23].
    m_chatBackdropQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(12, 14, 22, 195));
    m_chatBackdropQuad->setPosition(glm::vec2(-0.98f, -0.85f));
    m_chatBackdropQuad->setSize(glm::vec2(0.68f, 0.62f));

    chatTitle.setString("");
    chatTitle.setColor(SDL_Color{200, 200, 200, 255});
    chatTitle.setPosition(glm::vec2(-0.93f, -0.30f));

    chatInputText.setString("");
    chatInputText.setColor(SDL_Color{0, 200, 255, 255});
    chatInputText.setPosition(glm::vec2(-0.93f, -0.74f));

    chatMessageRows.resize(kChatMaxLines);
    const float startY = -0.38f;
    const float rowH = 0.048f;
    for (int i = 0; i < kChatMaxLines; ++i)
    {
        chatMessageRows[i] = std::make_unique<spear::ui::vulkan::Text>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue,
                m_descriptorPool, m_descriptorSetLayout, m_fontPath, 18);
        chatMessageRows[i]->setColor(SDL_Color{255, 255, 255, 255});
        chatMessageRows[i]->setPosition(glm::vec2(-0.93f, startY - i * rowH));
        chatMessageRows[i]->setString("");
    }
}

void Ui::setChatVisible(bool visible)
{
    m_chatVisible = visible;
}

void Ui::renderChatOverlay(spear::ui::RenderContext ctx)
{
    if (!m_chatVisible)
        return;
    if (m_chatBackdropQuad)
        m_chatBackdropQuad->render(ctx);
}

void Ui::setChatLine(int index, const std::string& content)
{
    if (index < 0 || index >= kChatMaxLines)
        return;
    auto& text = *chatMessageRows[index];

    // Only rebuild the texture when the content actually changed, otherwise
    // every frame causes a GPU stall.
    if (text.getString() != content)
        text.setString(content);
}

void Ui::clearChatRows()
{
    for (auto& row : chatMessageRows)
    {
        if (!row->getString().empty())
            row->setString("");
    }
}

void Ui::setChatInput(const std::string& text)
{
    if (chatInputText.getString() != text)
        chatInputText.setString(text);
}

void Ui::updatePlayerNameplates(const std::vector<PlayerNameplate>& players)
{
    std::unordered_set<uint64_t> active;
    for (const auto& player : players)
    {
        active.insert(player.id);
        auto& plate = m_playerNameplates[player.id];
        plate.visible = player.visible && !player.username.empty();
        if (!plate.visible)
            continue;

        if (!plate.text)
        {
            plate.text = std::make_unique<spear::ui::vulkan::Text>(
                    m_device, m_physDevice, m_commandPool, m_graphicsQueue,
                    m_descriptorPool, m_descriptorSetLayout, m_fontPath, 20, 0.002f);
        }
        if (plate.enemy != player.enemy)
        {
            plate.enemy = player.enemy;
            plate.text->setColor(player.enemy ? SDL_Color{255, 70, 70, 255}
                                              : SDL_Color{255, 255, 255, 255});
        }
        if (plate.text->getString() != player.username)
            plate.text->setString(player.username);

        const glm::vec2 size = plate.text->getSize();
        const glm::vec2 position(player.anchor.x - size.x * 0.5f,
                                 player.anchor.y + 0.025f);
        if (glm::distance(plate.text->getPosition(), position) > 0.003f)
            plate.text->setPosition(position);
    }

    for (auto it = m_playerNameplates.begin(); it != m_playerNameplates.end();)
        it = active.contains(it->first) ? std::next(it) : m_playerNameplates.erase(it);
}

void Ui::renderPlayerNameplates(spear::ui::RenderContext ctx)
{
    for (auto& [id, plate] : m_playerNameplates)
        if (plate.visible && plate.text)
            plate.text->render(ctx);
}

void Ui::setRowText(spear::ui::vulkan::Text& text, const LeaderboardEntry& entry)
{
    // Pad the name to a fixed width for column alignment.
    auto name = entry.username;
    if (name.size() > 18)
        name = name.substr(0, 18);
    else
        name.append(18 - name.size(), ' ');

    std::string content = name + std::to_string(entry.kills) + "   " + std::to_string(entry.deaths);
    // Only rebuild the texture if the content actually changed, otherwise
    // frequent leaderboard broadcasts cause constant GPU stalls/flicker.
    if (text.getString() != content)
        text.setString(content);
}

void Ui::showMenuTexts()
{
    clear();
    m_playerNameplates.clear();
    registerText(titleText);
    registerText(statusText);
    registerText(instructionsText);
    registerText(serverPlayerCountText);
}

void Ui::showServerInputTexts()
{
    clear();
    m_playerNameplates.clear();
    registerText(serverInputLabel);
    registerText(serverAddressText);
    registerText(usernameInputLabel);
    registerText(usernameInputText);
    registerText(serverInputInstructions);
}

void Ui::showTeamSelectTexts()
{
    clear();
    m_playerNameplates.clear();
    registerText(teamSelectTitle);
    registerText(teamSelectInstructions);
    registerText(ctButtonText);
    registerText(tButtonText);
    registerText(serverPlayerCountText);
    for (auto& row : teamRosterRows)
        registerText(*row);
}

void Ui::setTeamRoster(const std::vector<LeaderboardEntry>& entries)
{
    std::vector<LeaderboardEntry> ct;
    std::vector<LeaderboardEntry> t;
    for (const auto& entry : entries)
    {
        if (entry.team == "Counter Terrorist")
            ct.push_back(entry);
        else if (entry.team == "Terrorist")
            t.push_back(entry);
    }
    auto byName = [](const LeaderboardEntry& a, const LeaderboardEntry& b)
    {
        return a.username < b.username;
    };
    std::sort(ct.begin(), ct.end(), byName);
    std::sort(t.begin(), t.end(), byName);

    int row = 0;
    auto addSide = [&](const std::vector<LeaderboardEntry>& players, float leftX,
                       float startY, SDL_Color color)
    {
        for (std::size_t i = 0; i < players.size() && row < kTeamRosterMaxRows; ++i)
        {
            auto& text = *teamRosterRows[row++];
            const float x = leftX + (i / 15) * 0.41f;
            const float y = startY - (i % 15) * 0.05f;
            text.setPosition(glm::vec2(x, y));
            text.setColor(color);
            if (text.getString() != players[i].username)
                text.setString(players[i].username);
        }
    };
    addSide(ct, -0.78f, -0.08f,
            SDL_Color{135, 205, 255, 255});
    addSide(t, 0.16f, -0.08f,
            SDL_Color{255, 190, 115, 255});
    for (; row < kTeamRosterMaxRows; ++row)
        if (!teamRosterRows[row]->getString().empty())
            teamRosterRows[row]->setString("");
}

void Ui::showGameHudTexts()
{
    clear();
    registerText(healthText);
    registerText(serverPlayerCountText);
    registerText(weaponHudText);
    registerText(hitmarkerText);
    registerText(wastedText);
    registerText(teamDisplayText);
    registerText(roundText);
    registerText(matchStatusText);
    registerText(bombText);
    registerText(moneyText);
    registerText(fpsText);
    registerText(buyTitleText);
    registerText(buyHintText);
    registerText(buyNoticeText);
    for (auto& item : buyItemTexts)
        registerText(*item);
    registerText(pauseTitle);
    registerText(pauseTeamTitle);
    registerText(pauseCTText);
    registerText(pauseTText);
    registerText(pauseQuitText);
    registerText(pauseSettingsText);
    registerText(pauseInstructions);
    registerText(settingsTitle);
    registerText(settingsHint);
    registerText(settingsBack);
    for (auto& text : m_crosshairPreviewTexts)
        registerText(*text);
    for (auto& row : settingsRows)
        registerText(*row);
    for (auto& value : settingsValues)
        registerText(*value);
    for (auto& arrow : settingsLeftArrows)
        registerText(*arrow);
    for (auto& arrow : settingsRightArrows)
        registerText(*arrow);

    registerText(leaderboardTitle);
    registerText(leaderboardTHeader);
    registerText(leaderboardCTHeader);
    for (auto& row : leaderboardTRows)
        registerText(*row);
    for (auto& row : leaderboardCTRows)
        registerText(*row);

    registerText(chatTitle);
    registerText(chatInputText);
    for (auto& row : chatMessageRows)
        registerText(*row);
}

void Ui::buildBuyMenu()
{
    auto makeTexture = [&](unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        auto texture = std::make_shared<spear::rendering::vulkan::STBTexture>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue);
        unsigned char pixel[4] = {r, g, b, a};
        texture->loadFromRGBA(pixel, 1, 1);
        return texture;
    };
    m_buyBackdropQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeTexture(8, 14, 22, 218));
    m_buyBackdropQuad->setPosition(glm::vec2(-1.0f, -1.0f));
    m_buyBackdropQuad->setSize(glm::vec2(2.0f, 2.0f));
    m_buyPanelQuad = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeTexture(27, 42, 57, 245));
    m_buyPanelQuad->setPosition(glm::vec2(-0.80f, -0.77f));
    m_buyPanelQuad->setSize(glm::vec2(1.60f, 1.55f));

    auto cardTexture = makeTexture(45, 66, 82, 255);

    for (std::size_t i = 0; i < nx3d::economy::kItems.size(); ++i)
    {
        m_buyItemQuads.push_back(std::make_shared<spear::ui::vulkan::Quad2D>(
                m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
                cardTexture));
        auto text = std::make_unique<spear::ui::vulkan::Text>(
                m_device, m_physDevice, m_commandPool, m_graphicsQueue,
                m_descriptorPool, m_descriptorSetLayout, m_fontPath, 17);
        text->setString("");
        buyItemTexts.push_back(std::move(text));
    }
}

void Ui::showBuyMenu(BuyStyle style, const nx3d::economy::Snapshot& state,
                     const std::string& team, const std::string& notice,
                     bool bombMatch)
{
    m_buyVisible = true;
    m_buyStyle = style;
    const char* title = style == BuyStyle::List ? "BUY  |  LIST" :
                        style == BuyStyle::Radial ? "BUY  |  RADIAL" : "BUY  |  CS2 GRID";
    buyTitleText.setString(title);
    buyHintText.setString("Click to buy  |  Tab: Change layout  |  B / Esc: Close");
    buyNoticeText.setString(notice);
    constexpr float kPi = 3.14159265358979323846f;
    for (std::size_t i = 0; i < nx3d::economy::kItems.size(); ++i)
    {
        const auto& item = nx3d::economy::kItems[i];
        auto& text = *buyItemTexts[i];
        const bool available = (!*item.team || team == item.team) && state.money >= item.price &&
                               (item.kind != nx3d::economy::ItemKind::defuseKit || bombMatch) &&
                               (item.action != 19 || state.objects.count("grenade") < nx3d::economy::kMaxGrenades) &&
                               (item.action != 20 || state.objects.count("smoke") < nx3d::economy::kMaxSmokes);
        text.setColor(available ? SDL_Color{235, 245, 250, 255}
                                : SDL_Color{125, 142, 150, 255});
        text.setString(std::string(item.name) + "  $" + std::to_string(item.price));
        if (style == BuyStyle::List)
        {
            text.setPosition(glm::vec2(-0.63f, 0.40f - static_cast<float>(i) * 0.095f));
            m_buyItemQuads[i]->setPosition(glm::vec2(-0.68f, 0.36f - static_cast<float>(i) * 0.095f));
            m_buyItemQuads[i]->setSize(glm::vec2(1.35f, 0.085f));
        }
        else if (style == BuyStyle::Radial)
        {
            const float angle = -kPi * 0.5f + static_cast<float>(i) * 2.0f * kPi /
                                                       static_cast<float>(nx3d::economy::kItems.size());
            text.setPosition(glm::vec2(-0.15f + 0.57f * std::cos(angle),
                                       0.04f + 0.43f * std::sin(angle)));
            m_buyItemQuads[i]->setPosition(glm::vec2(-0.18f + 0.57f * std::cos(angle),
                                                     -0.01f + 0.43f * std::sin(angle)));
            m_buyItemQuads[i]->setSize(glm::vec2(0.34f, 0.10f));
        }
        else
        {
            text.setPosition(glm::vec2(-0.69f + static_cast<float>(i % 3) * 0.52f,
                                       0.34f - static_cast<float>(i / 3) * 0.26f));
            m_buyItemQuads[i]->setPosition(glm::vec2(-0.73f + static_cast<float>(i % 3) * 0.52f,
                                                     0.23f - static_cast<float>(i / 3) * 0.26f));
            m_buyItemQuads[i]->setSize(glm::vec2(0.50f, 0.22f));
        }
    }
}

void Ui::hideBuyMenu()
{
    m_buyVisible = false;
    buyTitleText.setString("");
    buyHintText.setString("");
    buyNoticeText.setString("");
    for (auto& text : buyItemTexts)
        text->setString("");
}

int Ui::buyItemAt(float x, float y) const
{
    if (!m_buyVisible)
        return -1;
    constexpr float kPi = 3.14159265358979323846f;
    if (m_buyStyle == BuyStyle::Radial)
    {
        const float dx = (x + 0.15f) / 0.57f;
        const float dy = (y - 0.04f) / 0.43f;
        const float radius = std::sqrt(dx * dx + dy * dy);
        if (radius < 0.42f || radius > 1.25f)
            return -1;
        const float step = 2.0f * kPi / static_cast<float>(nx3d::economy::kItems.size());
        const int sector = static_cast<int>(std::lround((std::atan2(dy, dx) + kPi * 0.5f) / step));
        return (sector % static_cast<int>(nx3d::economy::kItems.size()) +
                static_cast<int>(nx3d::economy::kItems.size())) %
               static_cast<int>(nx3d::economy::kItems.size());
    }
    for (std::size_t i = 0; i < nx3d::economy::kItems.size(); ++i)
    {
        if (m_buyStyle == BuyStyle::List)
        {
            const float rowY = 0.40f - static_cast<float>(i) * 0.095f;
            if (x >= -0.68f && x <= 0.63f && y >= rowY - 0.035f && y <= rowY + 0.055f)
                return static_cast<int>(i);
        }
        else
        {
            const float rowX = -0.69f + static_cast<float>(i % 3) * 0.52f;
            const float rowY = 0.34f - static_cast<float>(i / 3) * 0.26f;
            if (x >= rowX - 0.03f && x <= rowX + 0.48f &&
                y >= rowY - 0.08f && y <= rowY + 0.07f)
                return static_cast<int>(i);
        }
    }
    return -1;
}

void Ui::renderBuyOverlay(spear::ui::RenderContext ctx)
{
    if (!m_buyVisible)
        return;
    m_buyBackdropQuad->render(ctx);
    m_buyPanelQuad->render(ctx);
    for (auto& card : m_buyItemQuads)
        card->render(ctx);
}

void Ui::renderPauseOverlay(spear::ui::RenderContext ctx, bool quitHovered,
                            bool ctHovered, bool tHovered, bool settingsHovered)
{
    if (m_pauseBackdropQuad)
        m_pauseBackdropQuad->render(ctx);
    if (m_pausePanelBorder)
        m_pausePanelBorder->render(ctx);
    if (m_pausePanel)
        m_pausePanel->render(ctx);
    if (ctHovered)
    {
        if (m_pauseCTButtonHover)
            m_pauseCTButtonHover->render(ctx);
    }
    else if (m_pauseCTButton)
        m_pauseCTButton->render(ctx);
    if (tHovered)
    {
        if (m_pauseTButtonHover)
            m_pauseTButtonHover->render(ctx);
    }
    else if (m_pauseTButton)
        m_pauseTButton->render(ctx);
    if (settingsHovered)
        m_pauseSettingsButtonHover->render(ctx);
    else
        m_pauseSettingsButton->render(ctx);
    if (quitHovered)
    {
        if (m_pauseQuitButtonHover)
            m_pauseQuitButtonHover->render(ctx);
    }
    else if (m_pauseQuitButton)
    {
        m_pauseQuitButton->render(ctx);
    }
}

void Ui::renderSettingsOverlay(spear::ui::RenderContext ctx, int selectedRow)
{
    m_pauseBackdropQuad->render(ctx);
    m_settingsPanel->render(ctx);
    const int count = m_crosshairEditor ? kCrosshairRowCount : kSettingsRowCount;
    if (selectedRow >= 0 && selectedRow < count)
        m_settingsSelection->render(ctx);
    if (m_crosshairEditor)
    {
        m_crosshairPreviewPanels[m_crosshairBackground]->render(ctx);
        for (int row = 0; row < kCrosshairRowCount; ++row)
            if (m_crosshairSliderTracks[row])
            {
                m_crosshairSliderTracks[row]->render(ctx);
                m_crosshairSliderFills[row]->render(ctx);
            }
    }
}

void Ui::settingsLayout(bool crosshair)
{
    m_crosshairEditor = crosshair;
    settingsTitle.setPosition(crosshair ? glm::vec2(-0.87f, 0.74f) : glm::vec2(-0.23f, 0.64f));
    settingsBack.setPosition(crosshair ? glm::vec2(-0.87f, -0.78f) : glm::vec2(-0.17f, -0.59f));
    settingsHint.setPosition(crosshair ? glm::vec2(-0.87f, -0.85f) : glm::vec2(-0.62f, -0.71f));
    m_settingsPanel->setPosition(crosshair ? glm::vec2(-0.95f, -0.92f) : glm::vec2(-0.72f, -0.80f));
    m_settingsPanel->setSize(crosshair ? glm::vec2(1.90f, 1.84f) : glm::vec2(1.44f, 1.60f));
    m_settingsSelection->setSize(crosshair ? glm::vec2(1.10f, 0.082f) : glm::vec2(1.30f, 0.096f));
    const int count = crosshair ? kCrosshairRowCount : kSettingsRowCount;
    for (int i = 0; i < count; ++i)
    {
        const float y = crosshair ? 0.59f - i * 0.087f : 0.48f - i * 0.10f;
        settingsRows[i]->setPosition({crosshair ? -0.87f : -0.60f, y});
        settingsValues[i]->setPosition({crosshair ? -0.26f : 0.13f, y});
        settingsLeftArrows[i]->setPosition({crosshair ? -0.36f : -0.06f, y});
        settingsRightArrows[i]->setPosition({crosshair ? 0.16f : 0.55f, y});
    }
}

void Ui::selectSettingsRow(int row)
{
    if (row == m_settingsSelectedRow) return;
    if (m_settingsSelectedRow >= 0)
        settingsRows[m_settingsSelectedRow]->setColor({220, 230, 240, 255});
    settingsRows[row]->setColor({255, 220, 150, 255});
    m_settingsSelectedRow = row;
    m_settingsSelection->setPosition(m_crosshairEditor
        ? glm::vec2(-0.90f, 0.565f - row * 0.087f)
        : glm::vec2(-0.65f, 0.455f - row * 0.10f));
}

void Ui::showSettings(const std::array<std::string, kSettingsRowCount>& values, int selectedRow)
{
    static constexpr std::array<const char*, kSettingsRowCount> labels{{
            "Resolution", "Display mode", "Field of view", "Sound volume",
            "FPS limit", "Smoke detail", "Mouse sensitivity", "Show FPS", "Voicepack", "Crosshair"}};
    if (settingsTitle.getString() != "SETTINGS")
    {
        settingsLayout(false);
        settingsTitle.setString("SETTINGS");
    }
    if (settingsHint.getString() != "UP/DOWN: Select   LEFT/RIGHT: Change   ESC: Back")
        settingsHint.setString("UP/DOWN: Select   LEFT/RIGHT: Change   ESC: Back");
    if (settingsBack.getString() != "[ BACK ]") settingsBack.setString("[ BACK ]");
    for (int i = 0; i < kSettingsRowCount; ++i)
    {
        if (settingsRows[i]->getString() != labels[i]) settingsRows[i]->setString(labels[i]);
        if (settingsValues[i]->getString() != values[i]) settingsValues[i]->setString(values[i]);
        if (settingsLeftArrows[i]->getString() != "<") settingsLeftArrows[i]->setString("<");
        if (settingsRightArrows[i]->getString() != ">") settingsRightArrows[i]->setString(">");
    }
    selectSettingsRow(selectedRow);
}

void Ui::showCrosshairSettings(const std::array<std::string, kCrosshairRowCount>& values,
                              const std::array<float, kCrosshairRowCount>& sliders,
                              int selectedRow, int background)
{
    static constexpr std::array<const char*, kCrosshairRowCount> labels{{
        "Preset", "Style", "Length", "Thickness", "Gap", "Center dot", "T-style",
        "Draw outline", "Outline thickness", "Red", "Green", "Blue", "Opacity",
        "Preview background", "Restore defaults"}};
    if (settingsTitle.getString() != "CROSSHAIR")
    {
        settingsLayout(true);
        settingsTitle.setString("CROSSHAIR");
        settingsHint.setString("Arrows: Adjust   Drag sliders   ESC: Back");
        settingsBack.setString("[ BACK TO SETTINGS ]");
        constexpr std::array<const char*, 5> previewLabels{{
            "LIVE PREVIEW", "Actual size", "3x magnification", "SPACE: Test fire", "Changes saved automatically"}};
        for (int i = 0; i < 5; ++i) m_crosshairPreviewTexts[i]->setString(previewLabels[i]);
    }
    m_crosshairBackground = background;
    for (int i = 0; i < kCrosshairRowCount; ++i)
    {
        if (settingsRows[i]->getString() != labels[i]) settingsRows[i]->setString(labels[i]);
        if (settingsValues[i]->getString() != values[i]) settingsValues[i]->setString(values[i]);
        const char* left = i == 14 ? "" : "<";
        const char* right = i == 14 ? "" : ">";
        if (settingsLeftArrows[i]->getString() != left) settingsLeftArrows[i]->setString(left);
        if (settingsRightArrows[i]->getString() != right) settingsRightArrows[i]->setString(right);
        if (sliders[i] >= 0 && m_crosshairSliderFills[i])
        {
            const float width = std::max(0.001f, sliders[i] * 0.40f);
            if (std::abs(m_crosshairSliderFills[i]->getSize().x - width) > 0.0001f)
                m_crosshairSliderFills[i]->setSize({width, 0.009f});
        }
    }
    selectSettingsRow(selectedRow);
}

void Ui::hideSettings()
{
    settingsTitle.setString("");
    settingsHint.setString("");
    settingsBack.setString("");
    for (auto& row : settingsRows)
        row->setString("");
    for (auto& value : settingsValues)
        value->setString("");
    for (auto& arrow : settingsLeftArrows)
        arrow->setString("");
    for (auto& arrow : settingsRightArrows)
        arrow->setString("");
    for (auto& text : m_crosshairPreviewTexts) text->setString("");
    if (m_settingsSelectedRow >= 0)
        settingsRows[m_settingsSelectedRow]->setColor({220, 230, 240, 255});
    m_settingsSelectedRow = -1;
}

int Ui::settingsRowAt(float x, float y) const
{
    if (x < (m_crosshairEditor ? -0.90f : -0.65f) || x > (m_crosshairEditor ? 0.20f : 0.65f))
        return -1;
    const int count = m_crosshairEditor ? kCrosshairRowCount : kSettingsRowCount;
    for (int row = 0; row < count; ++row)
    {
        const float bottom = m_crosshairEditor ? 0.565f - row * 0.087f : 0.455f - row * 0.10f;
        if (y >= bottom && y < bottom + (m_crosshairEditor ? 0.087f : 0.10f)) return row;
    }
    return -1;
}

int Ui::settingsDirectionAt(float x) const
{
    if (m_crosshairEditor)
        return x < -0.30f ? -1 : 1;
    if (x >= -0.12f && x <= 0.06f) return -1;
    if (x >= 0.10f && x <= 0.65f) return 1;
    return 0;
}

bool Ui::crosshairSliderAt(int row, float x, float y) const
{
    if (!m_crosshairEditor || row < 0 || row >= kCrosshairRowCount || !m_crosshairSliderTracks[row]) return false;
    const float center = 0.59f - row * 0.087f - 0.008f;
    return x >= -0.29f && x <= 0.15f && std::abs(y - center) <= 0.018f;
}

void Ui::showLeaderboard(const std::vector<LeaderboardEntry>& entries)
{
    leaderboardTitle.setString("SCOREBOARD");
    leaderboardTHeader.setString("Terrorist");
    leaderboardCTHeader.setString("Counter Terrorist");

    std::vector<LeaderboardEntry> tEntries;
    std::vector<LeaderboardEntry> ctEntries;
    tEntries.reserve(kLeaderboardMaxRows);
    ctEntries.reserve(kLeaderboardMaxRows);
    for (const auto& entry : entries)
    {
        if (entry.team == "Terrorist")
            tEntries.push_back(entry);
        else if (entry.team == "Counter Terrorist")
            ctEntries.push_back(entry);
    }

    // Sort by kills descending so the top fraggers sit at the top of the table.
    auto byKills = [](const LeaderboardEntry& a, const LeaderboardEntry& b)
    {
        return a.kills > b.kills;
    };
    std::sort(tEntries.begin(), tEntries.end(), byKills);
    std::sort(ctEntries.begin(), ctEntries.end(), byKills);

    for (int i = 0; i < kLeaderboardMaxRows; ++i)
    {
        if (i < static_cast<int>(tEntries.size()))
            setRowText(*leaderboardTRows[i], tEntries[i]);
        else if (!leaderboardTRows[i]->getString().empty())
            leaderboardTRows[i]->setString("");
        if (i < static_cast<int>(ctEntries.size()))
            setRowText(*leaderboardCTRows[i], ctEntries[i]);
        else if (!leaderboardCTRows[i]->getString().empty())
            leaderboardCTRows[i]->setString("");
    }

    m_leaderboardVisible = true;
}

void Ui::hideLeaderboard()
{
    leaderboardTitle.setString("");
    leaderboardTHeader.setString("");
    leaderboardCTHeader.setString("");
    for (auto& row : leaderboardTRows)
        row->setString("");
    for (auto& row : leaderboardCTRows)
        row->setString("");
    m_leaderboardVisible = false;
}

void Ui::renderLeaderboardOverlay(spear::ui::RenderContext ctx)
{
    if (!m_leaderboardVisible)
        return;
    if (m_leaderboardBackdropQuad)
        m_leaderboardBackdropQuad->render(ctx);
    if (m_leaderboardPanelQuad)
        m_leaderboardPanelQuad->render(ctx);
}

} // namespace nx3d::client::ui
