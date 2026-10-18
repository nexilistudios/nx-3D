#ifndef NX3D_CLIENT_APP_CLIENT_APP_HH
#define NX3D_CLIENT_APP_CLIENT_APP_HH

#include <client/gamemode/game_mode.hh>
#include <client/audio/voice_pack.hh>
#include <client/ui/ui.hh>

#include <nexilis/client/client_api.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/room_info.hh>
#include <nexilis/tcp_client.hh>
#include <nexilis/udp_client.hh>

#include <spear/spear_vulkan.hh>

#include <btBulletDynamicsCommon.h>

#include <SDL3/SDL.h>

#include <atomic>
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nx3d::client
{

namespace audio
{
/// Sound identifiers used by positional audio events sent to / received from
/// the server. Kept as raw bytes on the wire; matched against these constants.
inline constexpr uint8_t kSoundFootstep = 0;
inline constexpr uint8_t kSoundShoot = 1;
inline constexpr uint8_t kSoundReload = 2;
inline constexpr uint8_t kSoundPlanting = 3;
inline constexpr uint8_t kSoundAk47 = 4;
inline constexpr uint8_t kSoundM16 = 5;
inline constexpr uint8_t kSoundGlock = 6;
inline constexpr uint8_t kSoundUsp = 7;
inline constexpr uint8_t kSoundDeagle = 8;
inline constexpr uint8_t kSoundMp5 = 9;
inline constexpr uint8_t kSoundDefusing = 10;
inline constexpr uint8_t shotSoundForWeapon(const std::string_view type)
{
    if (type == "m16") return kSoundM16;
    if (type == "glock") return kSoundGlock;
    if (type == "usp") return kSoundUsp;
    if (type == "deagle") return kSoundDeagle;
    if (type == "mp5") return kSoundMp5;
    return kSoundAk47;
}
} // namespace audio

/// Owns the entire client application: windowing, rendering, UI, networking and
/// the main game loop. Everything that is specific to how a room plays is
/// delegated to a gamemode that is chosen from the room the player joined.
class ClientApp
{
public:
    enum class State
    {
        ServerInput,
        Connecting,
        Lobby,
        Joining,
        TeamSelect,
        Game,
        Buy,
        Paused,
        Settings
    };

    ClientApp(const std::string& initialServerAddress, const std::string& initialUsername, bool connectOnStart);
    ~ClientApp();

    ClientApp(const ClientApp&) = delete;
    ClientApp& operator=(const ClientApp&) = delete;

    /// Run the main loop until the user quits.
    void run();

    // --- General rendering / scene infrastructure ---------------------------
    spear::VulkanWindow window;
    spear::Camera camera;
    spear::SceneManager scene_manager;
    spear::physics::bullet::World bullet_world;
    std::shared_ptr<btDiscreteDynamicsWorld> shared_world;
    /// Static triangle-mesh body used to test whether map geometry blocks a shot.
    btRigidBody* mapCollisionBody = nullptr;
    spear::MovementController movement_controller;
    spear::rendering::vulkan::Renderer renderer;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    spear::audio::AudioSystem audio_system;
    std::array<std::unique_ptr<spear::audio::Sound>, 6> gunshot_audio;
    spear::audio::Sound* gunshotSound(uint8_t sound);
    std::unique_ptr<spear::audio::Sound> hitmarker_audio;
    std::unique_ptr<spear::audio::Sound> reload_audio;
    std::unique_ptr<spear::audio::Sound> planting_audio;
    std::unique_ptr<spear::audio::Sound> defusing_audio;
    std::unique_ptr<spear::audio::Sound> bomb_beep_audio;
    std::unique_ptr<spear::audio::Sound> explosion_audio;
    std::array<std::array<std::unique_ptr<spear::audio::Sound>,
                          static_cast<std::size_t>(audio::VoiceCue::count)>,
               audio::kVoicePacks.size()> voicePackSounds;
    void playVoiceCue(audio::VoiceCue cue);
    std::unique_ptr<spear::audio::Sound> smoke_audio;
    std::unique_ptr<spear::audio::Sound> walk_audio;
    /// Short positional "step" clip for sounds other players make when
    /// walking. A single footfall cut from walk.mp3, played at world
    /// positions, so remote steps are spatialized and never collide with the
    /// local walk loop.
    std::unique_ptr<spear::audio::Sound> step_audio;
    spear::Time time_interface;
    spear::EventHandler eventHandler;
    std::unique_ptr<ui::Ui> ui;

    // --- Scenes -------------------------------------------------------------
    uint64_t lobby_scene_id = 0;
    uint64_t game_scene_id = 0;

    // --- Networking ---------------------------------------------------------
    std::string serverAddress;
    nexilis::ProtocolManager protocol_manager;
    nexilis::client::ClientAPI client_api;
    nexilis::TCPClient tcp_client;
    nexilis::UDPClient udp_client;
    std::atomic<bool> ready{false};
    float input_send_elapsed = 1.0f;
    std::mutex mtx;
    std::vector<nexilis::RoomInfo> rooms;
    std::atomic<bool> joiningRoom{false};
    uint64_t selectedRoomId = 0;
    nx3d::GameMode selectedRoomGameMode = nx3d::GameMode::deathmatch;
    float serverPlayerCountRefresh = 0.0f;
    std::unordered_map<uint64_t, ui::LeaderboardEntry> teamSelectPlayers;

    // --- Server address input ------------------------------------------------
    std::string serverAddressInput = "127.0.0.1";
    bool serverInputCleared = false;
    std::string lastServerDisplay;

    // --- Username input -------------------------------------------------------
    std::string username;
    std::string usernameInput;
    bool usernameInputCleared = false;
    std::string lastUsernameDisplay;
    /// 0 = server address field, 1 = username field.
    int activeTextInputField = 0;

    // --- Lobby room menu -----------------------------------------------------
    spear::ui::BaseMenuList* roomMenu = nullptr;
    bool menuPopulated = false;

    // --- Pause / audio -------------------------------------------------------
    float audioVolume = 0.1f;
    std::size_t voicePackIndex = 0;
    bool quitHovered = false;
    bool ctTeamHovered = false;
    bool tTeamHovered = false;
    bool settingsHovered = false;
    int settingsRow = 0;
    int resolutionIndex = 0;
    bool fullscreen = false;
    int fpsCapIndex = 2;
    int graphicsDetail = 2;
    bool showFps = false;
    float fpsDisplayTimer = 0.0f;
    int fpsDisplayFrames = 0;
    nx3d::economy::Snapshot economyState;
    ui::Ui::BuyStyle buyStyle = ui::Ui::BuyStyle::List;
    std::string buyNotice;
    std::string buyError;
    float buyErrorSeconds = 0.0f;

    // --- Textures -------------------------------------------------------------
    std::shared_ptr<spear::rendering::vulkan::STBTexture> wallnut_texture;
    std::shared_ptr<spear::rendering::vulkan::STBTexture> niilo_texture;
    std::shared_ptr<spear::rendering::vulkan::STBTexture> crosshair_texture;
    std::shared_ptr<spear::rendering::vulkan::STBTexture> smoke_texture;
    glm::vec3 default_size{1.0f};

    // --- Remote entities (network sync) --------------------------------------
    struct RemotePlayerVisual
    {
        // The complete model becomes the physics ragdoll on death. Living
        // players render separate parts so their arms and legs can move.
        std::shared_ptr<spear::rendering::vulkan::OBJModel> ragdoll;
        std::array<std::shared_ptr<spear::rendering::vulkan::OBJModel>, 5> parts;
        std::shared_ptr<spear::rendering::vulkan::OBJModel> weapon;
        std::shared_ptr<spear::rendering::vulkan::OBJModel> magazine;
        std::string weaponType;
        std::shared_ptr<spear::rendering::vulkan::OBJModel> heldItem;
        std::string heldItemType;
    };
    std::unordered_map<uint64_t, RemotePlayerVisual> remote_players;
    std::unordered_map<uint64_t, std::string> remote_player_teams;
    struct RemotePlayerAnimation
    {
        glm::vec3 position{0.0f};
        float facing = 0.0f;
        float stride = 0.0f;
        float speed = 0.0f;
        float hitTime = 0.0f;
        std::string locomotion = "idle";
        std::string weapon;
        std::string action;
        float actionRemaining = 0.0f;
        float actionDuration = 0.0f;
        bool authoritative = false;
        bool initialized = false;
    };
    struct DeathAnimation
    {
        std::shared_ptr<spear::rendering::vulkan::OBJModel> model;
        glm::vec3 position{0.0f};
        float facing = 0.0f;
        float elapsed = 0.0f;
    };
    std::unordered_map<uint64_t, RemotePlayerAnimation> remote_player_animations;
    std::unordered_set<uint64_t> remote_dead_players;
    std::vector<DeathAnimation> death_animations;
    float player_animation_clock = 0.0f;
    std::unordered_map<uint64_t, std::shared_ptr<spear::rendering::vulkan::TexturedCube>> remote_objects;
    std::unordered_map<uint64_t, std::shared_ptr<spear::rendering::vulkan::OBJModel>> remote_game_items;
    std::vector<std::shared_ptr<spear::GameObject>> pendingDestroy[3];
    int frameCount = 0;

    // --- Active gamemode --------------------------------------------------------
    State currentState = State::ServerInput;
    std::unique_ptr<gamemode::GameMode> game_mode;

    // --- Chat ---------------------------------------------------------------------
    /// Whether the chat input line is open (typing "y" toggles it).
    bool chatActive = false;
    /// The text currently being typed (limited to chat::kMaxMessageCharacters).
    std::string chatInput;
    /// Last string pushed to the chat input line (blinking cursor diffing).
    std::string lastChatDisplay;
    /// One chat line as shown in the window: sender username + payload.
    struct ChatMessage
    {
        std::string username;
        std::string payload;
    };
    /// Rolling history of the most recent chat messages (newest at the back).
    std::vector<ChatMessage> chatMessages;
    /// Number of nexilis room messages already consumed into `chatMessages`.
    std::size_t chatMessagesConsumed = 0;
    /// Room whose messages `chatMessagesConsumed` refers to (reset on room change).
    uint64_t chatRoomId = 0;
    /// Seconds the chat window stays visible after the last received message.
    float chatVisibleTimer = 0.0f;

    /// How long the chat window stays visible after a new message arrives.
    static constexpr float kChatDisplaySeconds = 6.0f;
    /// Maximum chat lines shown at once (matches ui::Ui::kChatMaxLines).
    static constexpr int kChatMaxLines = 6;

    /// Open the chat input line (takes over keyboard input from the game).
    void openChat();
    /// Close the chat input line; sends the typed message when `send` is true.
    void closeChat(bool send);
    /// Consume new room messages from nexilis into `chatMessages`.
    void updateChatMessages();
    /// Count down the visibility timer and refresh the chat window UI.
    void updateChatWindow(float delta_time);
    void updateTeamSelectRoster();

    // --- Helpers ----------------------------------------------------------------
    void connect(const std::string& address);
    void enterGame(const std::string& team);
    void animatePlayerHit(uint64_t playerId);
    void animatePlayerDeath(uint64_t playerId);
    void animatePlayerRespawn(uint64_t playerId);
    void retireRemotePlayerParts(RemotePlayerVisual& visual);
    void setRemotePlayerWeapon(RemotePlayerVisual& visual, const std::string& weapon,
                               const glm::vec3& position);
    void applyAudioVolume();
    void changeVolume(float delta);
    void pauseGame();
    void resumeGame();
    void openSettings();
    void closeSettings();
    void changeSetting(int row, int direction);
    void refreshSettings();
    void loadSettings();
    void saveSettings() const;
    void openBuyMenu();
    void closeBuyMenu();
    void cleanQuit();

private:
    std::unique_ptr<gamemode::GameMode> makeGameMode(nx3d::GameMode gameMode);
    void setupScenes();
    void syncRemotePlayerTransforms(float deltaTime);
    void syncRemoteObjects();
    void syncRemoteGameItems();
};

} // namespace nx3d::client

#endif
