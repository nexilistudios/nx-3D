#ifndef NX3D_CLIENT_APP_CLIENT_APP_HH
#define NX3D_CLIENT_APP_CLIENT_APP_HH

#include <client/gamemode/game_mode.hh>
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
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nx3d::client
{

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
        Paused
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
    spear::MovementController movement_controller;
    spear::rendering::vulkan::Renderer renderer;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    spear::audio::AudioSystem audio_system;
    std::unique_ptr<spear::audio::Sound> gunshot_audio;
    std::unique_ptr<spear::audio::Sound> hitmarker_audio;
    std::unique_ptr<spear::audio::Sound> walk_audio;
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
    std::mutex mtx;
    std::vector<nexilis::RoomInfo> rooms;
    std::atomic<bool> joiningRoom{false};
    uint64_t selectedRoomId = 0;
    nx3d::GameMode selectedRoomGameMode = nx3d::GameMode::deathmatch;

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
    bool quitHovered = false;

    // --- Textures -------------------------------------------------------------
    std::shared_ptr<spear::rendering::vulkan::STBTexture> wallnut_texture;
    std::shared_ptr<spear::rendering::vulkan::STBTexture> niilo_texture;
    std::shared_ptr<spear::rendering::vulkan::STBTexture> crosshair_texture;
    glm::vec3 default_size{1.0f};

    // --- Remote entities (network sync) --------------------------------------
    std::unordered_map<uint64_t, std::shared_ptr<spear::rendering::vulkan::TexturedCube>> remote_players;
    std::unordered_map<uint64_t, std::shared_ptr<spear::rendering::vulkan::TexturedCube>> remote_objects;
    std::unordered_map<uint64_t, std::shared_ptr<spear::rendering::vulkan::OBJModel>> remote_game_items;
    std::vector<std::shared_ptr<spear::GameObject>> pendingDestroy[3];
    int frameCount = 0;

    // --- Active gamemode --------------------------------------------------------
    State currentState = State::ServerInput;
    std::unique_ptr<gamemode::GameMode> game_mode;

    // --- Helpers ----------------------------------------------------------------
    void connect(const std::string& address);
    void enterGame(const std::string& team);
    void applyAudioVolume();
    void changeVolume(float delta);
    void setVolumeNormalized(float normalized);
    void pauseGame();
    void resumeGame();
    void cleanQuit();

private:
    std::unique_ptr<gamemode::GameMode> makeGameMode(nx3d::GameMode gameMode);
    void setupScenes();
    void syncRemotePlayerTransforms();
    void syncRemoteObjects();
    void syncRemoteGameItems();
};

} // namespace nx3d::client

#endif
