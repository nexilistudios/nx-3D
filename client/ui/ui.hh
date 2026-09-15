#ifndef NX3D_CLIENT_UI_UI_HH
#define NX3D_CLIENT_UI_UI_HH

#include <spear/spear_vulkan.hh>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nx3d::client::ui
{

/// One row of the in-game leaderboard.
struct LeaderboardEntry
{
    uint64_t id = 0;
    std::string username;
    std::string team;
    uint64_t kills = 0;
    uint64_t deaths = 0;
};

/// All of the user interface elements of the client: the menu screens,
/// the lobby, the team select, the in-game HUD and the pause menu.
///
/// Everything is built once here so the game logic can simply poke at the
/// exposed text/quad members instead of re-creating Vulkan resources.
class Ui
{
public:
    explicit Ui(VkDevice device,
                VkPhysicalDevice physDevice,
                VkCommandPool commandPool,
                VkQueue graphicsQueue,
                VkDescriptorPool descriptorPool,
                VkDescriptorSetLayout descriptorSetLayout,
                const std::string& fontPath);

    spear::ui::vulkan::UIRenderer& renderer()
    {
        return m_renderer;
    }

    void clear()
    {
        m_renderer.clear();
    }

    void registerText(spear::ui::BaseText& text)
    {
        m_renderer.addExternalText(text);
    }

    spear::ui::BaseMenuList& createRoomMenu()
    {
        return m_renderer.createMenuList();
    }

    /// Register the texts shared by the connecting / lobby screens.
    void showMenuTexts();
    /// Register the server address input screen.
    void showServerInputTexts();
    /// Register the team select screen (clears previous UI).
    void showTeamSelectTexts();
    /// Register the in-game HUD (clears previous UI).
    void showGameHudTexts();

    /// Render the pause menu overlay (quads only; used from the overlay
    /// callback that also draws the crosshair).
    void renderPauseOverlay(spear::ui::RenderContext ctx, bool quitHovered);

    /// Show the Tab leaderboard with the given entries. Terrorists are shown
    /// on the left, Counter Terrorists on the right.
    void showLeaderboard(const std::vector<LeaderboardEntry>& entries);
    /// Hide the Tab leaderboard.
    void hideLeaderboard();
    /// Whether the Tab leaderboard is currently shown.
    bool isLeaderboardVisible() const
    {
        return m_leaderboardVisible;
    }
    /// Render the leaderboard backdrop/panel quads (used from the overlay
    /// callback; the row text itself is registered as regular UI text).
    void renderLeaderboardOverlay(spear::ui::RenderContext ctx);

    /// Synchronize the volume slider/pause text with the given volume [0,1].
    void updateVolumeDisplay(float volume);

    // --- Text elements -----------------------------------------------------
    spear::ui::vulkan::Text titleText;
    spear::ui::vulkan::Text statusText;
    spear::ui::vulkan::Text instructionsText;

    spear::ui::vulkan::Text serverInputLabel;
    spear::ui::vulkan::Text serverAddressText;
    spear::ui::vulkan::Text serverInputInstructions;

    spear::ui::vulkan::Text usernameInputLabel;
    spear::ui::vulkan::Text usernameInputText;

    spear::ui::vulkan::Text ctButtonText;
    spear::ui::vulkan::Text tButtonText;
    spear::ui::vulkan::Text teamSelectTitle;
    spear::ui::vulkan::Text teamSelectInstructions;

    spear::ui::vulkan::Text healthText;
    spear::ui::vulkan::Text weaponHudText;
    spear::ui::vulkan::Text hitmarkerText;
    spear::ui::vulkan::Text teamDisplayText;

    spear::ui::vulkan::Text pauseTitle;
    spear::ui::vulkan::Text pauseVolumeText;
    spear::ui::vulkan::Text pauseQuitText;
    spear::ui::vulkan::Text pauseInstructions;

    // --- Leaderboard (Tab) --------------------------------------------------
    spear::ui::vulkan::Text leaderboardTitle;
    spear::ui::vulkan::Text leaderboardTHeader;
    spear::ui::vulkan::Text leaderboardCTHeader;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> leaderboardTRows;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> leaderboardCTRows;

    /// Maximum rows shown per team on the leaderboard.
    static constexpr int kLeaderboardMaxRows = 10;

private:
    void buildPauseMenu();
    void buildLeaderboard();
    void setRowText(spear::ui::vulkan::Text& text, const LeaderboardEntry& entry);

    VkDevice m_device;
    VkPhysicalDevice m_physDevice;
    VkCommandPool m_commandPool;
    VkQueue m_graphicsQueue;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSetLayout m_descriptorSetLayout;
    std::string m_fontPath;

    spear::ui::vulkan::UIRenderer m_renderer;

    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseBackdropQuad;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pausePanelBorder;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pausePanel;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseSliderTrack;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseSliderFill;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseSliderKnob;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseQuitButton;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseQuitButtonHover;

    std::shared_ptr<spear::ui::vulkan::Quad2D> m_leaderboardBackdropQuad;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_leaderboardPanelQuad;
    bool m_leaderboardVisible = false;
};

} // namespace nx3d::client::ui

#endif