#ifndef NX3D_CLIENT_UI_UI_HH
#define NX3D_CLIENT_UI_UI_HH

#include <spear/spear_vulkan.hh>
#include <shared/economy.hh>

#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
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

struct PlayerNameplate
{
    uint64_t id = 0;
    std::string username;
    glm::vec2 anchor{0.0f};
    bool visible = false;
    bool enemy = false;
};

/// All of the user interface elements of the client: the menu screens,
/// the lobby, the team select, the in-game HUD and the pause menu.
///
/// Everything is built once here so the game logic can simply poke at the
/// exposed text/quad members instead of re-creating Vulkan resources.
class Ui
{
public:
    static constexpr int kSettingsRowCount = 10;
    static constexpr int kCrosshairRowCount = 15;
    enum class BuyStyle { List, Radial, Cs2 };
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
    /// Show the players already assigned to each side on the team screen.
    void setTeamRoster(const std::vector<LeaderboardEntry>& entries);
    void showBuyMenu(BuyStyle style, const nx3d::economy::Snapshot& state,
                     const std::string& team, const std::string& notice,
                     bool bombMatch);
    void hideBuyMenu();
    int buyItemAt(float x, float y) const;
    void renderBuyOverlay(spear::ui::RenderContext ctx);

    /// Render the pause menu overlay (quads only; used from the overlay
    /// callback that also draws the crosshair).
    void renderPauseOverlay(spear::ui::RenderContext ctx, bool quitHovered,
                            bool ctHovered, bool tHovered, bool settingsHovered);
    void renderSettingsOverlay(spear::ui::RenderContext ctx, int selectedRow);
    void showSettings(const std::array<std::string, kSettingsRowCount>& values, int selectedRow);
    void showCrosshairSettings(const std::array<std::string, kCrosshairRowCount>& values,
                              const std::array<float, kCrosshairRowCount>& sliders,
                              int selectedRow, int background);
    void hideSettings();
    int settingsRowAt(float x, float y) const;
    int settingsDirectionAt(float x) const;
    bool crosshairSliderAt(int row, float x, float y) const;
    void renderDeathOverlay(spear::ui::RenderContext ctx);

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

    // --- Text elements -----------------------------------------------------
    spear::ui::vulkan::Text titleText;
    spear::ui::vulkan::Text statusText;
    spear::ui::vulkan::Text instructionsText;
    spear::ui::vulkan::Text serverPlayerCountText;

    spear::ui::vulkan::Text serverInputLabel;
    spear::ui::vulkan::Text serverAddressText;
    spear::ui::vulkan::Text serverInputInstructions;

    spear::ui::vulkan::Text usernameInputLabel;
    spear::ui::vulkan::Text usernameInputText;

    spear::ui::vulkan::Text ctButtonText;
    spear::ui::vulkan::Text tButtonText;
    spear::ui::vulkan::Text teamSelectTitle;
    spear::ui::vulkan::Text teamSelectInstructions;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> teamRosterRows;

    static constexpr int kTeamRosterMaxRows = 30;

    spear::ui::vulkan::Text healthText;
    spear::ui::vulkan::Text weaponHudText;
    spear::ui::vulkan::Text hitmarkerText;
    spear::ui::vulkan::Text wastedText;
    spear::ui::vulkan::Text teamDisplayText;
    spear::ui::vulkan::Text roundText;
    spear::ui::vulkan::Text matchStatusText;
    spear::ui::vulkan::Text bombText;
    spear::ui::vulkan::Text moneyText;
    spear::ui::vulkan::Text fpsText;
    spear::ui::vulkan::Text buyTitleText;
    spear::ui::vulkan::Text buyHintText;
    spear::ui::vulkan::Text buyNoticeText;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> buyItemTexts;

    spear::ui::vulkan::Text pauseTitle;
    spear::ui::vulkan::Text pauseTeamTitle;
    spear::ui::vulkan::Text pauseCTText;
    spear::ui::vulkan::Text pauseTText;
    spear::ui::vulkan::Text pauseQuitText;
    spear::ui::vulkan::Text pauseSettingsText;
    spear::ui::vulkan::Text pauseInstructions;
    spear::ui::vulkan::Text settingsTitle;
    spear::ui::vulkan::Text settingsHint;
    spear::ui::vulkan::Text settingsBack;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> settingsRows;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> settingsValues;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> settingsLeftArrows;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> settingsRightArrows;

    // --- Leaderboard (Tab) --------------------------------------------------
    spear::ui::vulkan::Text leaderboardTitle;
    spear::ui::vulkan::Text leaderboardTHeader;
    spear::ui::vulkan::Text leaderboardCTHeader;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> leaderboardTRows;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> leaderboardCTRows;

    /// Maximum rows shown per team on the leaderboard.
    static constexpr int kLeaderboardMaxRows = 10;

    // --- Chat (bottom-left corner) ------------------------------------------
    spear::ui::vulkan::Text chatTitle;
    spear::ui::vulkan::Text chatInputText;
    /// Message rows, oldest at the top. Composed as "username: payload".
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> chatMessageRows;

    /// Maximum chat messages shown in the window at once.
    static constexpr int kChatMaxLines = 6;

    /// Show/hide the chat window (backdrop + rows get filled from the game).
    void setChatVisible(bool visible);
    /// Render the chat backdrop quad (used from the overlay callback).
    void renderChatOverlay(spear::ui::RenderContext ctx);
    /// Update one visual chat line (oldest at the top). An empty string clears it.
    void setChatLine(int index, const std::string& content);
    /// Clear all chat message rows.
    void clearChatRows();
    /// Set the text shown on the chat input line (empty hides it).
    void setChatInput(const std::string& text);

    void updatePlayerNameplates(const std::vector<PlayerNameplate>& players);
    void renderPlayerNameplates(spear::ui::RenderContext ctx);

private:
    void buildPauseMenu();
    void buildDeathOverlay();
    void buildLeaderboard();
    void buildChat();
    void buildBuyMenu();
    void setRowText(spear::ui::vulkan::Text& text, const LeaderboardEntry& entry);

    VkDevice m_device;
    VkPhysicalDevice m_physDevice;
    VkCommandPool m_commandPool;
    VkQueue m_graphicsQueue;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSetLayout m_descriptorSetLayout;
    std::string m_fontPath;

    spear::ui::vulkan::UIRenderer m_renderer;
    struct NameplateText
    {
        std::unique_ptr<spear::ui::vulkan::Text> text;
        bool visible = false;
        bool enemy = false;
    };
    std::unordered_map<uint64_t, NameplateText> m_playerNameplates;

    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseBackdropQuad;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_deathBackdropQuad;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_deathBandQuad;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pausePanelBorder;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pausePanel;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseCTButton;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseCTButtonHover;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseTButton;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseTButtonHover;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseQuitButton;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseQuitButtonHover;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseSettingsButton;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_pauseSettingsButtonHover;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_settingsPanel;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_settingsSelection;
    int m_settingsSelectedRow = -1;
    bool m_crosshairEditor = false;
    int m_crosshairBackground = 0;
    void settingsLayout(bool crosshair);
    void selectSettingsRow(int row);
    std::array<std::shared_ptr<spear::ui::vulkan::Quad2D>, 3> m_crosshairPreviewPanels;
    std::array<std::shared_ptr<spear::ui::vulkan::Quad2D>, kCrosshairRowCount> m_crosshairSliderTracks;
    std::array<std::shared_ptr<spear::ui::vulkan::Quad2D>, kCrosshairRowCount> m_crosshairSliderFills;
    std::vector<std::unique_ptr<spear::ui::vulkan::Text>> m_crosshairPreviewTexts;

    std::shared_ptr<spear::ui::vulkan::Quad2D> m_leaderboardBackdropQuad;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_leaderboardPanelQuad;
    bool m_leaderboardVisible = false;

    std::shared_ptr<spear::ui::vulkan::Quad2D> m_chatBackdropQuad;
    bool m_chatVisible = false;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_buyBackdropQuad;
    std::shared_ptr<spear::ui::vulkan::Quad2D> m_buyPanelQuad;
    std::vector<std::shared_ptr<spear::ui::vulkan::Quad2D>> m_buyItemQuads;
    bool m_buyVisible = false;
    BuyStyle m_buyStyle = BuyStyle::List;
};

} // namespace nx3d::client::ui

#endif
