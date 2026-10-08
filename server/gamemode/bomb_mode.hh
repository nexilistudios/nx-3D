#ifndef NX3D_SERVER_GAMEMODE_BOMB_MODE_HH
#define NX3D_SERVER_GAMEMODE_BOMB_MODE_HH

#include <server/gamemode/game_mode.hh>

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>

namespace nx3d::server::gamemode
{

/// One MR12 match in one room. All state changes are serialized because room
/// commands arrive on client threads while update() runs on the server tick.
class BombMode : public GameMode
{
public:
    nx3d::GameMode type() const override
    {
        return nx3d::GameMode::bomb;
    }
    void onPlayerDeath(nexilis::server::Room& room, uint64_t killerId, uint64_t victimId) override;
    void onTeamSelected(nexilis::server::Room& room, uint64_t playerId) override;
    bool onAction(nexilis::server::Room& room, uint64_t playerId, uint8_t action) override;
    bool canDamage(uint64_t shooterId, uint64_t targetId) const override;
    bool canSelectTeam(uint64_t playerId) const override;
    void update(nexilis::server::Room& room, float dt) override;

private:
    enum class Phase
    {
        waiting,
        live,
        post,
        finished
    };
    static constexpr size_t kMaxPlayersPerTeam = 5;
    void publish(nexilis::server::Room& room);
    void startRound(nexilis::server::Room& room);
    void endRound(nexilis::server::Room& room, bool terroristsWin, const std::string& reason);
    void reconcile(nexilis::server::Room& room);
    bool teamAlive(nexilis::server::Room& room, const std::string& team) const;
    bool closeToBomb(uint64_t playerId) const;
    void clearAction();

    mutable std::mutex m_mutex;
    std::deque<uint64_t> m_queue;
    std::unordered_set<uint64_t> m_players;
    std::unordered_set<uint64_t> m_alive;
    Phase m_phase = Phase::waiting;
    unsigned m_round = 0;
    unsigned m_tScore = 0;
    unsigned m_ctScore = 0;
    float m_seconds = 0.0f;
    float m_publishClock = 0.0f;
    uint64_t m_actionPlayer = 0;
    uint8_t m_action = 0;
    float m_actionSeconds = 0.0f;
    bool m_planted = false;
    std::atomic<bool> m_swapping{false};
    float m_bombX = 0.0f, m_bombY = 0.0f, m_bombZ = 0.0f;
    std::string m_notice = "Waiting for both teams";
};

} // namespace nx3d::server::gamemode
#endif
