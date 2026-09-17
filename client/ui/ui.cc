#include <client/ui/ui.hh>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

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
      teamDisplayText(device, physDevice, commandPool, graphicsQueue,
                      descriptorPool, descriptorSetLayout, fontPath, 20),

      pauseTitle(device, physDevice, commandPool, graphicsQueue,
                 descriptorPool, descriptorSetLayout, fontPath, 48),
      pauseVolumeText(device, physDevice, commandPool, graphicsQueue,
                      descriptorPool, descriptorSetLayout, fontPath, 24),
      pauseQuitText(device, physDevice, commandPool, graphicsQueue,
                    descriptorPool, descriptorSetLayout, fontPath, 28),
      pauseInstructions(device, physDevice, commandPool, graphicsQueue,
                        descriptorPool, descriptorSetLayout, fontPath, 14),

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

    buildLeaderboard();
    buildChat();
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

    teamSelectInstructions.setString("1 = CT    2 = T    |    Click a team to join");
    teamSelectInstructions.setColor(SDL_Color{200, 200, 200, 255});
    teamSelectInstructions.setPosition(glm::vec2(-0.25f, -0.3f));

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

    teamDisplayText.setString("");
    teamDisplayText.setColor(SDL_Color{255, 255, 255, 255});
    teamDisplayText.setPosition(glm::vec2(0.65f, -0.95f));

    // --- Pause menu ----------------------------------------------------------
    pauseTitle.setString("");
    pauseTitle.setColor(SDL_Color{255, 255, 255, 255});
    pauseTitle.setPosition(glm::vec2(-0.17f, 0.47f));

    pauseVolumeText.setString("");
    pauseVolumeText.setColor(SDL_Color{255, 200, 0, 255});
    pauseVolumeText.setPosition(glm::vec2(-0.17f, 0.17f));

    pauseQuitText.setString("");
    pauseQuitText.setColor(SDL_Color{255, 255, 255, 255});
    pauseQuitText.setPosition(glm::vec2(-0.22f, -0.33f));

    pauseInstructions.setString("");
    pauseInstructions.setColor(SDL_Color{200, 200, 200, 255});
    pauseInstructions.setPosition(glm::vec2(-0.37f, -0.56f));

    buildPauseMenu();
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

    m_pauseSliderTrack = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(90, 90, 100, 255));
    m_pauseSliderTrack->setPosition(glm::vec2(-0.45f, 0.0f));
    m_pauseSliderTrack->setSize(glm::vec2(0.9f, 0.07f));

    m_pauseSliderFill = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(255, 150, 0, 255));
    m_pauseSliderFill->setPosition(glm::vec2(-0.45f, 0.0f));
    m_pauseSliderFill->setSize(glm::vec2(0.9f, 0.07f));

    m_pauseSliderKnob = std::make_shared<spear::ui::vulkan::Quad2D>(
            m_device, m_physDevice, m_descriptorPool, m_descriptorSetLayout,
            makeSolidTexture(255, 210, 0, 255));
    m_pauseSliderKnob->setSize(glm::vec2(0.05f, 0.15f));

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

void Ui::setChatRow(int index, const std::string& username, const std::string& payload)
{
    if (index < 0 || index >= kChatMaxLines)
        return;
    auto& text = *chatMessageRows[index];

    std::string content;
    if (!username.empty() || !payload.empty())
        content = username + ": " + payload;
    // Keep the line inside the chat panel (monospace FiraCode at ~18pt).
    if (content.size() > 34)
        content = content.substr(0, 34);

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
    registerText(titleText);
    registerText(statusText);
    registerText(instructionsText);
}

void Ui::showServerInputTexts()
{
    clear();
    registerText(serverInputLabel);
    registerText(serverAddressText);
    registerText(usernameInputLabel);
    registerText(usernameInputText);
    registerText(serverInputInstructions);
}

void Ui::showTeamSelectTexts()
{
    clear();
    registerText(teamSelectTitle);
    registerText(teamSelectInstructions);
    registerText(ctButtonText);
    registerText(tButtonText);
}

void Ui::showGameHudTexts()
{
    clear();
    registerText(healthText);
    registerText(weaponHudText);
    registerText(hitmarkerText);
    registerText(teamDisplayText);
    registerText(pauseTitle);
    registerText(pauseVolumeText);
    registerText(pauseQuitText);
    registerText(pauseInstructions);

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

void Ui::renderPauseOverlay(spear::ui::RenderContext ctx, bool quitHovered)
{
    if (m_pauseBackdropQuad)
        m_pauseBackdropQuad->render(ctx);
    if (m_pausePanelBorder)
        m_pausePanelBorder->render(ctx);
    if (m_pausePanel)
        m_pausePanel->render(ctx);
    if (m_pauseSliderTrack)
        m_pauseSliderTrack->render(ctx);
    if (m_pauseSliderFill)
        m_pauseSliderFill->render(ctx);
    if (m_pauseSliderKnob)
        m_pauseSliderKnob->render(ctx);
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

void Ui::updateVolumeDisplay(float volume)
{
    int percent = static_cast<int>(std::lround(volume * 100.0f));
    pauseVolumeText.setString("Volume: " + std::to_string(percent) + "%");
    float fillWidth = 0.9f * volume;
    if (m_pauseSliderFill)
        m_pauseSliderFill->setSize(glm::vec2(fillWidth, 0.07f));
    if (m_pauseSliderKnob)
        m_pauseSliderKnob->setPosition(glm::vec2(-0.45f + fillWidth - 0.025f, -0.04f));
}

} // namespace nx3d::client::ui