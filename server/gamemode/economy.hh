#ifndef NX3D_SERVER_GAMEMODE_ECONOMY_HH
#define NX3D_SERVER_GAMEMODE_ECONOMY_HH

#include <shared/economy.hh>

#include <nexilis/command_type.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/room.hh>

#include <boost/json/value.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nx3d::server::gamemode
{

/// Per-room wallets and equipment. All purchases are checked on the server.
class Economy
{
public:
    void joined(nexilis::server::Room& room, uint64_t id)
    {
        nx3d::economy::Snapshot state;
        {
            std::lock_guard lock(m_mutex);
            auto& wallet = m_wallets[id];
            wallet.weapon = nx3d::economy::defaultWeapon(room.getPlayerTeam(id));
            wallet.primary.clear();
            wallet.secondary = wallet.weapon;
            wallet.objects = {};
            state = wallet;
        }
        send(room, id, state);
    }

    void respawned(nexilis::server::Room& room, uint64_t id,
                   bool issueBomb = false, bool preserveEquipment = false)
    {
        nx3d::economy::Snapshot state;
        {
            std::lock_guard lock(m_mutex);
            auto& wallet = m_wallets[id];
            if (!preserveEquipment)
            {
                wallet.weapon = nx3d::economy::defaultWeapon(room.getPlayerTeam(id));
                wallet.primary.clear();
                wallet.secondary = wallet.weapon;
                wallet.objects = {};
            }
            else
                wallet.objects.set("bomb", 0);
            wallet.effect = "none";
            if (issueBomb && room.getPlayerTeam(id) == "Terrorist")
                wallet.objects.add("bomb");
            state = wallet;
            if (issueBomb && room.getPlayerTeam(id) == "Terrorist")
                state.effect = "bomb_spawn";
            else if (preserveEquipment)
                state.effect = "survivor_spawn";
        }
        send(room, id, state);
    }

    void resetMatch(nexilis::server::Room& room, const std::unordered_set<uint64_t>& players)
    {
        for (auto id : players)
        {
            nx3d::economy::Snapshot state;
            state.weapon = nx3d::economy::defaultWeapon(room.getPlayerTeam(id));
            state.secondary = state.weapon;
            {
                std::lock_guard lock(m_mutex);
                m_wallets[id] = state;
            }
            send(room, id, state);
        }
    }

    void died(nexilis::server::Room& room, uint64_t killer, uint64_t victim)
    {
        nx3d::economy::Snapshot victimState;
        nx3d::economy::Snapshot killerState;
        bool awarded = false;
        bool lostKit = false;
        {
            std::lock_guard lock(m_mutex);
            auto& victimWallet = m_wallets[victim];
            if (victimWallet.weapon.empty())
                victimWallet.weapon = nx3d::economy::defaultWeapon(room.getPlayerTeam(victim));
            lostKit = victimWallet.objects.count("defuse_kit") > 0 ||
                      victimWallet.objects.count("grenade") > 0 ||
                      victimWallet.objects.count("smoke") > 0 ||
                      victimWallet.objects.count("bomb") > 0;
            victimWallet.objects = {};
            victimState = victimWallet;
            if (killer != 0 && killer != victim)
            {
                auto& killerWallet = m_wallets[killer];
                if (killerWallet.weapon.empty())
                    killerWallet.weapon = nx3d::economy::defaultWeapon(room.getPlayerTeam(killer));
                credit(killerWallet, nx3d::economy::kKillReward);
                killerState = killerWallet;
                killerState.notice = "+$" + std::to_string(nx3d::economy::kKillReward) + " kill reward";
                awarded = true;
            }
        }
        if (lostKit)
            send(room, victim, victimState);
        if (awarded)
            send(room, killer, killerState);
    }

    void reward(nexilis::server::Room& room, uint64_t id, int amount, std::string reason)
    {
        nx3d::economy::Snapshot state;
        {
            std::lock_guard lock(m_mutex);
            auto& wallet = m_wallets[id];
            if (wallet.weapon.empty())
                wallet.weapon = nx3d::economy::defaultWeapon(room.getPlayerTeam(id));
            credit(wallet, amount);
            state = wallet;
        }
        state.notice = "+$" + std::to_string(amount) + " " + reason;
        send(room, id, state);
    }

    bool hasDefuseKit(uint64_t id) const
    {
        std::lock_guard lock(m_mutex);
        const auto it = m_wallets.find(id);
        return it != m_wallets.end() && it->second.objects.count("defuse_kit") > 0;
    }

    bool hasBomb(uint64_t id) const
    {
        std::lock_guard lock(m_mutex);
        const auto it = m_wallets.find(id);
        return it != m_wallets.end() && it->second.objects.count("bomb") > 0;
    }

    void setBomb(nexilis::server::Room& room, uint64_t id, bool owned)
    {
        if (owned && room.getPlayerTeam(id) != "Terrorist")
            return;
        nx3d::economy::Snapshot state;
        {
            std::lock_guard lock(m_mutex);
            auto it = m_wallets.find(id);
            if (it == m_wallets.end()) return;
            it->second.objects.set("bomb", owned ? 1 : 0);
            state = it->second;
        }
        send(room, id, state);
    }

    bool pickupBomb(nexilis::server::Room& room, uint64_t id)
    {
        if (room.getPlayerTeam(id) != "Terrorist" || room.getPlayerHealth(id) <= 0.0f)
            return false;
        nx3d::economy::Snapshot state;
        {
            std::lock_guard lock(m_mutex);
            auto it = m_wallets.find(id);
            if (it == m_wallets.end() || !it->second.objects.add("bomb"))
                return false;
            state = it->second;
            state.effect = "bomb_pickup";
        }
        send(room, id, state);
        return true;
    }

    std::string weaponForSlot(uint64_t id, uint8_t slot) const
    {
        if (slot == 3)
            return "knife";
        std::lock_guard lock(m_mutex);
        const auto it = m_wallets.find(id);
        if (it == m_wallets.end())
            return {};
        if (slot == 1)
            return it->second.primary;
        if (slot == 2)
            return it->second.secondary;
        if (slot == 4 && it->second.objects.count("grenade") > 0)
            return "grenade";
        if (slot == 5 && it->second.objects.count("smoke") > 0)
            return "smoke";
        if (slot == 6 && it->second.objects.count("bomb") > 0)
            return "bomb";
        if (slot == 7 && it->second.objects.count("defuse_kit") > 0)
            return "defuse_kit";
        return {};
    }

    bool pickupWeapon(nexilis::server::Room& room, uint64_t id, uint8_t slot,
                      const std::string& weapon)
    {
        if (room.getPlayerHealth(id) <= 0.0f || room.getPlayerTeam(id).empty())
            return false;
        nx3d::economy::Snapshot state;
        {
            std::lock_guard lock(m_mutex);
            auto it = m_wallets.find(id);
            if (it == m_wallets.end())
                return false;
            auto& equipped = slot == 1 ? it->second.primary : it->second.secondary;
            if (!equipped.empty())
                return false;
            equipped = weapon;
            it->second.weapon = weapon;
            state = it->second;
        }
        send(room, id, state);
        return true;
    }

    bool buy(nexilis::server::Room& room, uint64_t id, uint8_t action, bool alive,
             bool bombMatch = true)
    {
        const auto* item = nx3d::economy::itemForAction(action);
        if (!item)
            return false;

        auto* user = nexilis::server::ClientStorage::getClientById(id);
        if (!user)
            return false;
        const auto team = room.getPlayerTeam(id);
        const auto pos = user->getObject3D().getPosition();
        nx3d::economy::Snapshot state;
        bool success = false;
        {
            std::lock_guard lock(m_mutex);
            auto& wallet = m_wallets[id];
            if (wallet.weapon.empty())
                wallet.weapon = nx3d::economy::defaultWeapon(team);
            if (!alive || room.getPlayerHealth(id) <= 0.0f)
                state.notice = "You must be alive to buy";
            else if (!nx3d::economy::inBuyZone(team, pos.x, pos.y, pos.z))
                state.notice = "Buy only in your team spawn";
            else if (*item->team && team != item->team)
                state.notice = "Unavailable for your team";
            else if (item->kind == nx3d::economy::ItemKind::defuseKit && !bombMatch)
                state.notice = "Defuse kits are for bomb matches";
            else if (wallet.money < item->price)
                state.notice = "Not enough money";
            else if (item->kind == nx3d::economy::ItemKind::defuseKit &&
                     wallet.objects.count("defuse_kit") > 0)
                state.notice = "Already have a defuse kit";
            else if (item->object->id == "grenade" &&
                     wallet.objects.count("grenade") >= item->object->effect.maxStack)
                state.notice = "Grenade slots full";
            else if (item->object->id == "smoke" &&
                     wallet.objects.count("smoke") >= item->object->effect.maxStack)
                state.notice = "Smoke slots full";
            else if (item->kind == nx3d::economy::ItemKind::health && room.getPlayerHealth(id) >= 100.0f)
                state.notice = "Health already full";
            else
            {
                wallet.money -= item->price;
                if (item->kind == nx3d::economy::ItemKind::weapon)
                {
                    wallet.weapon = item->weapon;
                    if (nx3d::economy::isSecondary(item->weapon))
                        wallet.secondary = item->weapon;
                    else
                        wallet.primary = item->weapon;
                }
                else if (item->kind == nx3d::economy::ItemKind::defuseKit)
                    wallet.objects.add("defuse_kit");
                else if (item->kind == nx3d::economy::ItemKind::grenade)
                    wallet.objects.add(item->object->id);
                success = true;
                state.notice = std::string("Bought ") + item->name;
                state.effect = item->kind == nx3d::economy::ItemKind::weapon ? "weapon" :
                               item->kind == nx3d::economy::ItemKind::ammo ? "ammo" :
                               item->kind == nx3d::economy::ItemKind::health ? "health" : "none";
            }
            state.money = wallet.money;
            state.weapon = wallet.weapon;
            state.objects = wallet.objects;
            state.primary = wallet.primary;
            state.secondary = wallet.secondary;
        }
        if (success && item->kind == nx3d::economy::ItemKind::health)
            room.resetPlayerHealth(id);
        send(room, id, state);
        return success;
    }

    bool throwGrenade(nexilis::server::Room& room, uint64_t id, bool alive, bool smoke = false)
    {
        auto* user = nexilis::server::ClientStorage::getClientById(id);
        if (!user || !alive || room.getPlayerHealth(id) <= 0.0f)
            return false;
        nx3d::economy::Snapshot state;
        nx3d::economy::GrenadeThrow throwEvent;
        {
            std::lock_guard lock(m_mutex);
            auto& wallet = m_wallets[id];
            const auto type = smoke ? "smoke" : "grenade";
            if (!wallet.objects.consume(type))
                return false;
            state = wallet;
            state.effect = smoke ? "smoke_thrown" : "grenade_thrown";
            state.notice = smoke ? "Smoke thrown" : "Grenade thrown";
            const auto pos = user->getObject3D().getPosition();
            const float yaw = user->getObject3D().getYaw();
            throwEvent = {id, pos.x, pos.y, pos.z,
                          pos.x - std::sin(yaw) * nx3d::economy::kGrenadeThrowDistance,
                          pos.z - std::cos(yaw) * nx3d::economy::kGrenadeThrowDistance, type};
            if (!smoke)
                m_thrown.push_back({id, room.getPlayerTeam(id),
                                    throwEvent.endX, pos.y, throwEvent.endZ,
                                    nx3d::economy::kGrenadeFuseSeconds});
        }
        send(room, id, state);
        nexilis::server::Command::ClientMsgType data{
                {"action", "broadcast"}, {"room_id", room.getId()},
                {"client_id", 0},
                {"message", boost::json::value(nx3d::economy::encodeGrenade(throwEvent))}};
        room.broadcastToAll(nexilis::server::Command::clientMessageData(
                nexilis::CommandType::room, "communication", 0, data));
        return true;
    }

    bool dropWeapon(nexilis::server::Room& room, uint64_t id, uint8_t action)
    {
        if (room.getPlayerHealth(id) <= 0.0f || room.getPlayerTeam(id).empty())
            return false;
        nx3d::economy::Snapshot state;
        {
            std::lock_guard lock(m_mutex);
            auto it = m_wallets.find(id);
            if (it == m_wallets.end())
                return false;
            auto& wallet = it->second;
            std::string* equipped = action == nx3d::economy::kDropPrimaryAction ? &wallet.primary :
                                    action == nx3d::economy::kDropSecondaryAction ? &wallet.secondary : nullptr;
            if (!equipped || equipped->empty())
                return false;
            equipped->clear();
            state = wallet;
        }
        send(room, id, state);
        return true;
    }

    void updateGrenades(nexilis::server::Room& room, float dt)
    {
        std::vector<ThrownGrenade> exploded;
        {
            std::lock_guard lock(m_mutex);
            for (auto it = m_thrown.begin(); it != m_thrown.end();)
            {
                it->seconds -= dt;
                if (it->seconds <= 0.0f)
                {
                    exploded.push_back(*it);
                    it = m_thrown.erase(it);
                }
                else ++it;
            }
        }
        for (const auto& grenade : exploded)
        {
            auto* owner = nexilis::server::ClientStorage::getClientById(grenade.owner);
            if (!owner)
                continue;
            for (auto target : room.getClients())
            {
                if (target == grenade.owner || room.getPlayerTeam(target) == grenade.team ||
                    !room.canDamage(grenade.owner, target) || room.getPlayerHealth(target) <= 0.0f)
                    continue;
                auto* victim = nexilis::server::ClientStorage::getClientById(target);
                if (!victim)
                    continue;
                const auto pos = victim->getObject3D().getPosition();
                const float dx = pos.x - grenade.x, dy = pos.y - grenade.y, dz = pos.z - grenade.z;
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (distance >= nx3d::economy::kGrenadeBlastRadius)
                    continue;
                const float damage = nx3d::economy::kGrenadeMaxDamage *
                                     (1.0f - distance / nx3d::economy::kGrenadeBlastRadius);
                const bool killed = room.damagePlayer(target, damage);
                nexilis::nx_data damageData{
                        static_cast<uint8_t>(nexilis::CommandType::room),
                        static_cast<uint8_t>(nexilis::RoomCommandType::Root::player_3D),
                        static_cast<uint8_t>(nexilis::RoomCommandType::PlayerType::shoot)};
                nexilis::server::Command::ClientMsgType params{
                        {"target_id", boost::json::value(target)},
                        {"damage", boost::json::value(static_cast<double>(damage))},
                        {"new_health", boost::json::value(static_cast<double>(room.getPlayerHealth(target)))}};
                room.broadcastToAll(nexilis::server::Command::createRoomCommand(
                        room.getId(), *owner, damageData, params, 0));
                if (killed)
                    room.onPlayerDied(grenade.owner, target);
            }
        }
    }

    void reconcile(const std::vector<uint64_t>& clients)
    {
        std::lock_guard lock(m_mutex);
        for (auto it = m_wallets.begin(); it != m_wallets.end();)
            if (std::find(clients.begin(), clients.end(), it->first) == clients.end())
                it = m_wallets.erase(it);
            else
                ++it;
    }

private:
    struct ThrownGrenade
    {
        uint64_t owner;
        std::string team;
        float x, y, z;
        float seconds;
    };
    static void credit(nx3d::economy::Snapshot& wallet, int amount)
    {
        wallet.money = std::min(nx3d::economy::kMaximumMoney, wallet.money + amount);
    }

    static void send(nexilis::server::Room& room, uint64_t id, const nx3d::economy::Snapshot& state)
    {
        auto* user = nexilis::server::ClientStorage::getClientById(id);
        if (!user || !user->isBoostTCPSet())
            return;
        // Nexilis room messages carry application-specific per-player data.
        // A zero sender ID marks this as server data; the client consumes the
        // reserved prefix before it reaches the visible player chat.
        nexilis::server::Command::ClientMsgType data{
                {"action", "broadcast"}, {"room_id", room.getId()},
                {"client_id", 0},
                {"message", boost::json::value(nx3d::economy::encode(state))}};
        user->boostTCPSend(nexilis::server::Command::clientMessageData(
                nexilis::CommandType::room, "communication", 0, data));
    }

    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, nx3d::economy::Snapshot> m_wallets;
    std::vector<ThrownGrenade> m_thrown;
};

} // namespace nx3d::server::gamemode

#endif
